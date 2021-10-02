/*********************************************************************
* ARIA SYSTEMS RESEARCH GROUP
* 
* The main source code of KD-CBS
*********************************************************************/
 
/* Author: Justin Kottinger */

#include "../includes/KD_CBS.h"


namespace base = ompl::base;

// constructor
ompl::control::KD_CBS::KD_CBS(const std::vector<SimpleSetup> mmpp) : base::Planner(mmpp[0].getSpaceInformation(), "KD-CBS")
{
   for (SimpleSetup ss: mmpp)
      mmpp_.push_back(ss);
   // These params are for benchmarking -- ignore for now
   // Planner::declareParam<double>("goal_bias", this, &RRT::setGoalBias, &RRT::getGoalBias, "0.:.05:1.");
   // Planner::declareParam<bool>("intermediate_states", this, &RRT::setIntermediateStates, &RRT::getIntermediateStates,
                                // "0,1");
}

// de-constructor (TO-DO)
ompl::control::KD_CBS::~KD_CBS()
{
    // freeMemory();
}

void ompl::control::KD_CBS::interpolate(PathControl &p)
{
   if (p.getStates().size() <= p.getControls().size())
   {
      OMPL_ERROR("Interpolation not performed.  Number of states in the path should be strictly greater than the "
                   "number of controls.");
      return;
   }
 
   const auto *si = static_cast<const SpaceInformation *>(p.getSpaceInformation().get());
   std::vector<base::State *> newStates;
   std::vector<Control *> newControls;
   std::vector<double> newControlDurations;
 
   double res = si->getPropagationStepSize();
   for (unsigned int i = 0; i < p.getControls().size(); ++i)
   {
       auto steps = (int)floor(0.5 + p.getControlDuration(i) / res);
       assert(steps >= 0);
       if (steps <= 1)
       {
           newStates.push_back(p.getState(i));
           newControls.push_back(p.getControl(i));
           newControlDurations.push_back(p.getControlDuration(i));
           continue;
       }
       std::vector<base::State *> istates;
       si->propagate(p.getState(i), p.getControl(i), steps, istates, true);
       // last state is already in the non-interpolated path
       if (!istates.empty())
       {
           si_->freeState(istates.back());
           istates.pop_back();
       }
       newStates.push_back(p.getState(i));
       newStates.insert(newStates.end(), istates.begin(), istates.end());
       newControls.push_back(p.getControl(i));
       newControlDurations.push_back(res);
       for (int j = 1; j < steps; ++j)
       {
           newControls.push_back(si->cloneControl(p.getControl(i)));
           newControlDurations.push_back(res);
       }
   }
   newStates.push_back(p.getState(p.getControls().size()));
   p.getStates().swap(newStates);
   p.getControls().swap(newControls);
   p.getControlDurations().swap(newControlDurations);
}




std::unordered_set <Conflict> ompl::control::KD_CBS::validatePlan(Plan pl)
{
   // init the conflict set to be (possibly) filled
   std::unordered_set <Conflict> c;
   
   // get minimum step size
   const auto *si = static_cast<const SpaceInformation *>(pl[0].getSpaceInformation().get());
   const double minStepSize = si->getPropagationStepSize();

   // interpolate all trajectories to the same discretization
   for (int i = 0; i < pl.size(); i++)
      interpolate(pl[i]);
   
   // get longest trajectory -- i.e. max states
   int maxStates = 0;
   for (int i = 0; i < pl.size(); i++)
   {
      if (maxStates < pl[i].getStateCount())
         maxStates = pl[i].getStateCount();
   }
   
   // iterate through all the states
   for (int k = 0; k < maxStates; k++)
   {
      // for each k, get all the (valid) states for each valid agent
      std::vector<base::State *> validStatesAtK;
      std::vector<int> validAgentsIdx;
      for (int i = 0; i < pl.size(); i++)
      {
         if (k < pl[i].getStateCount())
         {
            validStatesAtK.push_back(pl[i].getState(k));
            validAgentsIdx.push_back(i);
         }
      }
      // for each valid agent at k, create agent shape
      std::vector<polygon> shapes;
      for (int j = 0; j < validAgentsIdx.size(); j++)
      {
         // get agent information from world
         const Agent *a = w_->getAgents()[validAgentsIdx[j]];
         const double carWidth = a->getShape()[0];
         const double carHeight = a->getShape()[1];
         // extract points from state object
         auto compState = validStatesAtK[j]->as<ob::CompoundStateSpace::StateType>();
         auto xyState = compState->as<ob::RealVectorStateSpace::StateType>(0);
         const double cx = xyState->values[0];
         const double cy = xyState->values[1];
         const double theta = compState->as<ob::SO2StateSpace::StateType>(1)->value;

         // turn (x,y, theta), width, length to a polygon object
         // borrowed from conflictChecking
         // TOP RIGHT VERTEX:
         const double TR_x = cx + ((carWidth / 2) * cos(theta)) - ((carHeight / 2) * sin(theta));
         const double TR_y = cy + ((carWidth / 2) * sin(theta)) + ((carHeight / 2) * cos(theta));
         std::string top_right = std::to_string(TR_x) + " " + std::to_string(TR_y);
         // TOP LEFT VERTEX:
         const double TL_x = cx - ((carWidth / 2) * cos(theta)) - ((carHeight / 2) * sin(theta));
         const double TL_y = cy - ((carWidth / 2) * sin(theta)) + ((carHeight / 2) * cos(theta));
         std::string top_left = std::to_string(TL_x) + " " + std::to_string(TL_y);
         // BOTTOM LEFT VERTEX:
         const double BL_x = cx - ((carWidth / 2) * cos(theta)) + ((carHeight / 2) * sin(theta));
         const double BL_y = cy - ((carWidth / 2) * sin(theta)) - ((carHeight / 2) * cos(theta));
         std::string bottom_left = std::to_string(BL_x) + " " + std::to_string(BL_y);
         // BOTTOM RIGHT VERTEX:
         const double BR_x = cx + ((carWidth / 2) * cos(theta)) + ((carHeight / 2) * sin(theta));
         const double BR_y = cy + ((carWidth / 2) * sin(theta)) - ((carHeight / 2) * cos(theta));
         std::string bottom_right = std::to_string(BR_x) + " " + std::to_string(BR_y);
         // convert to string for easy initializataion
         std::string points = "POLYGON((" + bottom_left + "," + bottom_right + "," + top_right + "," + top_left + "," + bottom_left + "))";
         polygon agent;
         boost::geometry::read_wkt(points,agent);
         shapes.push_back(agent);
      }
      // disjoint check the shapes for a collision
      for (int ai = 0; ai < shapes.size(); ai++)
      {
         for (int aj = 0; aj < shapes.size(); aj++)
         {
            if (ai != aj)
            {
               if (! boost::geometry::disjoint(shapes[ai], shapes[aj]))
               {
                  // agent ai and aj are in conflict, only care about those now
                  Conflict conf{ai, aj, shapes[ai], shapes[aj], (k * minStepSize)};
                  c.insert(conf);
                  bool inConflict = true;
                  while (inConflict)
                  {
                     k++;
                     // for each k, get the (valid) states for each valid agent
                     std::vector<base::State *> validStatesAtK;
                     if (k < pl[ai].getStateCount())
                        validStatesAtK.push_back(pl[ai].getState(k));
                     if (k < pl[aj].getStateCount())
                        validStatesAtK.push_back(pl[aj].getState(k));
                     if (validStatesAtK.size() < 2)
                        inConflict = false;
                     else
                     {
                        // get first agent information from world
                        const Agent *a1 = w_->getAgents()[validAgentsIdx[ai]];
                        const double carWidth1 = a1->getShape()[0];
                        const double carHeight1 = a1->getShape()[1];
                        // extract points from state object
                        auto compState1 = validStatesAtK[0]->as<ob::CompoundStateSpace::StateType>();
                        auto xyState1 = compState1->as<ob::RealVectorStateSpace::StateType>(0);
                        const double cx1 = xyState1->values[0];
                        const double cy1 = xyState1->values[1];
                        const double theta1 = compState1->as<ob::SO2StateSpace::StateType>(1)->value;
                        // turn (x,y, theta), width, length to a polygon object
                        // borrowed from conflictChecking
                        // TOP RIGHT VERTEX:
                        const double TR_x1 = cx1 + ((carWidth1 / 2) * cos(theta1)) - ((carHeight1 / 2) * sin(theta1));
                        const double TR_y1 = cy1 + ((carWidth1 / 2) * sin(theta1)) + ((carHeight1 / 2) * cos(theta1));
                        std::string top_right1 = std::to_string(TR_x1) + " " + std::to_string(TR_y1);
                        // TOP LEFT VERTEX:
                        const double TL_x1 = cx1 - ((carWidth1 / 2) * cos(theta1)) - ((carHeight1 / 2) * sin(theta1));
                        const double TL_y1 = cy1 - ((carWidth1 / 2) * sin(theta1)) + ((carHeight1 / 2) * cos(theta1));
                        std::string top_left1 = std::to_string(TL_x1) + " " + std::to_string(TL_y1);
                        // BOTTOM LEFT VERTEX:
                        const double BL_x1 = cx1 - ((carWidth1 / 2) * cos(theta1)) + ((carHeight1 / 2) * sin(theta1));
                        const double BL_y1 = cy1 - ((carWidth1 / 2) * sin(theta1)) - ((carHeight1 / 2) * cos(theta1));
                        std::string bottom_left1 = std::to_string(BL_x1) + " " + std::to_string(BL_y1);
                        // BOTTOM RIGHT VERTEX:
                        const double BR_x1 = cx1 + ((carWidth1 / 2) * cos(theta1)) + ((carHeight1 / 2) * sin(theta1));
                        const double BR_y1 = cy1 + ((carWidth1 / 2) * sin(theta1)) - ((carHeight1 / 2) * cos(theta1));
                        std::string bottom_right1 = std::to_string(BR_x1) + " " + std::to_string(BR_y1);
                        // convert to string for easy initializataion
                        std::string points1 = "POLYGON((" + bottom_left1 + "," + bottom_right1 + "," + top_right1 + "," + top_left1 + "," + bottom_left1 + "))";
                        polygon agent1;
                        boost::geometry::read_wkt(points1,agent1);

                        // get second agent information from world
                        const Agent *a2 = w_->getAgents()[validAgentsIdx[aj]];
                        const double carWidth2 = a2->getShape()[0];
                        const double carHeight2 = a2->getShape()[1];
                        // extract points from state object
                        auto compState2 = validStatesAtK[1]->as<ob::CompoundStateSpace::StateType>();
                        auto xyState2 = compState2->as<ob::RealVectorStateSpace::StateType>(0);
                        const double cx2 = xyState2->values[0];
                        const double cy2 = xyState2->values[1];
                        const double theta2 = compState2->as<ob::SO2StateSpace::StateType>(1)->value;
                        // turn (x,y, theta), width, length to a polygon object
                        // borrowed from conflictChecking
                        // TOP RIGHT VERTEX:
                        const double TR_x2 = cx2 + ((carWidth2 / 2) * cos(theta2)) - ((carHeight2 / 2) * sin(theta2));
                        const double TR_y2 = cy2 + ((carWidth2 / 2) * sin(theta2)) + ((carHeight2 / 2) * cos(theta2));
                        std::string top_right2 = std::to_string(TR_x2) + " " + std::to_string(TR_y2);
                        // TOP LEFT VERTEX:
                        const double TL_x2 = cx2 - ((carWidth2 / 2) * cos(theta2)) - ((carHeight2 / 2) * sin(theta2));
                        const double TL_y2 = cy2 - ((carWidth2 / 2) * sin(theta2)) + ((carHeight2 / 2) * cos(theta2));
                        std::string top_left2 = std::to_string(TL_x2) + " " + std::to_string(TL_y2);
                        // BOTTOM LEFT VERTEX:
                        const double BL_x2 = cx2 - ((carWidth2 / 2) * cos(theta2)) + ((carHeight2 / 2) * sin(theta2));
                        const double BL_y2 = cy2 - ((carWidth2 / 2) * sin(theta2)) - ((carHeight2 / 2) * cos(theta2));
                        std::string bottom_left2 = std::to_string(BL_x2) + " " + std::to_string(BL_y2);
                        // BOTTOM RIGHT VERTEX:
                        const double BR_x2 = cx2 + ((carWidth2 / 2) * cos(theta2)) + ((carHeight2 / 2) * sin(theta2));
                        const double BR_y2 = cy2 + ((carWidth2 / 2) * sin(theta2)) - ((carHeight2 / 2) * cos(theta2));
                        std::string bottom_right2 = std::to_string(BR_x2) + " " + std::to_string(BR_y2);
                        // convert to string for easy initializataion
                        std::string points2 = "POLYGON((" + bottom_left2 + "," + bottom_right2 + "," + top_right2 + "," + top_left2 + "," + bottom_left2 + "))";
                        polygon agent2;
                        boost::geometry::read_wkt(points2,agent2);

                        // check if agetnai and agent aj are disjoint at next time step
                        if (! boost::geometry::disjoint(agent1, agent2))
                        {
                           // agent ai and aj are in conflict, only care about those now
                           Conflict conf{ai, aj, agent1, agent2, (k * minStepSize)};
                           c.insert(conf);
                        }
                        else
                           inConflict = false;
                     }

                  }
                  return c;
               }
            }
         }
      }
   }
   return c;
}

// the main algorithm
base::PlannerStatus ompl::control::KD_CBS::solve(const base::PlannerTerminationCondition &ptc)
{ 
   /* verify assumption that step size is the same for each planner */
   const auto *si = static_cast<const SpaceInformation *>(mmpp_[0].getSpaceInformation().get());
   const double minStepSize = si->getPropagationStepSize();
   for (SimpleSetup ss: mmpp_)
   {
      const auto *si = static_cast<const SpaceInformation *>(ss.getSpaceInformation().get());
      const double tmpDT = si->getPropagationStepSize();
      if (tmpDT != minStepSize)
      {
         OMPL_ERROR("Planner Step Sizes are not the same for all agents!!");
         return base::PlannerStatus::INVALID_START;
      }
   }

   // begin planning -- timing should start after this statement
   OMPL_INFORM("%s: Starting planning with %u states already in datastructure", getName().c_str(), queue_.size());

   // create initial solution
   Plan root_sol;
   for (SimpleSetup ss: mmpp_)
   {
      base::PlannerStatus solved = ss.solve(planningTime_);
      ss.getPlanner()->clear();
      if (solved)
         root_sol.push_back(ss.getSolutionPath());
   }
   // create root node
   conflictNode *rootNode = new conflictNode();
   if (root_sol.size() == mmpp_.size())
   {
      rootNode->updatePlanAndCost(root_sol);
      queue_.insert(rootNode);
   }
   // if no root node exists, tell user and return invalid start
   if (queue_.size() == 0)
   {
       OMPL_ERROR("%s: There are no valid initial states! Increase planning time.", getName().c_str(), planningTime_);
       return base::PlannerStatus::INVALID_START;
   }
 
   // initialize solution, approximate solution, and approximate difference between approx and sol.
   conflictNode *solution = nullptr;
   conflictNode *approxsol = nullptr;
  
   while (ptc == false && !queue_.empty())
   { 
      // /* find loswest cost in Queue */
      conflictNode *curr = popHead();

      std::unordered_set <Conflict> c = validatePlan(curr->getPlan());

      if (c.empty())
      {
         solution = curr;
         break;
      }
      else
      {
         // create constraint for each agent and resolve them
         OMPL_WARN("%s: Original solution had %i conflicts.", getName().c_str(), c.size());
         exit(1);
      }
   }
   // end main algorithm -- begin ompl bookkeeping 
   bool solved = false;
   if (solution == nullptr)
   {
      OMPL_INFORM("%s: No solution found.", getName().c_str());
      return {solved, false};
   }
   else
   {
      // recognize solution, planning time stops here
      OMPL_INFORM("%s: Found Solution!", getName().c_str());
      // add correct path to all problem instances
      for (int i = 0; i < mmpp_.size(); i++)
      {
         base::ProblemDefinition *pdef = mmpp_[i].getProblemDefinition().get();
         auto path(std::make_shared<PathControl>(mmpp_[i].getSpaceInformation()));
         PathControl mpath = solution->getPlan()[i];
         // std::cout << mpath.getStates().size() << std::endl;
         // std::cout << mpath.getControls().size() << std::endl;
         // std::cout << mpath.getControlDurations().size() << std::endl;
         // std::cout << mpath.getStateCount() << std::endl;
         // std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
         for (int j = 0; j < mpath.getStateCount() - 1; j++)
         {
            if (j > 0)
            {
               // std::cout << mpath.getState(j) << std::endl;
               // std::cout << mpath.getControl(j) << std::endl;
               // std::cout << mpath.getControlDuration(j) << std::endl;
               path->append(mpath.getState(j), mpath.getControl(j), mpath.getControlDuration(j));
            }
            else
               path->append(mpath.getState(j));
         }
         solved = true;
         pdef->addSolutionPath(path, false, -1.0, w_->getAgents()[i]->getName());
      }
      OMPL_INFORM("%s: Planning Complete.", getName().c_str());
      return {solved, false};
   }

   // // solution is either approxsol or solution from main loop
   // if (solution != nullptr)
   // {
   //    // set last node to solution
   //    lastGoalNode_ = solution;
 
   //    /* construct the solution path */
   //    std::vector<Motion *> mpath;
   //    while (solution != nullptr)
   //    {
   //       mpath.push_back(solution);
   //       solution = solution->parent;
   //    }
 
   //    /* set the solution path */
   //    auto path(std::make_shared<PathControl>(si_));
   //    for (int i = mpath.size() - 1; i >= 0; --i)
   //       if (mpath[i]->parent)
   //          path->append(mpath[i]->state, mpath[i]->control, mpath[i]->steps * siC_->getPropagationStepSize());
   //       else
   //          path->append(mpath[i]->state);
   //    solved = true;
   //    pdef_->addSolutionPath(path, approximate, approxdif, getName());
   // }
 
   // if (rmotion->state)
   //    si_->freeState(rmotion->state);
   // if (rmotion->control)
   //    siC_->freeControl(rmotion->control);
   // delete rmotion;
   // si_->freeState(xstate);
}