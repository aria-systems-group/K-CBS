#pragma once
#include "Instance.h"
#include "ompl/base/spaces/RealVectorStateSpace.h"
#include "ompl/base/spaces/SO2StateSpace.h"
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/strategies/transform/matrix_transformers.hpp>


OMPL_CLASS_FORWARD(Instance);
class RealVectorStateSpaceSVC : public ob::StateValidityChecker
{
public:
    RealVectorStateSpaceSVC(const oc::SpaceInformationPtr &si, InstancePtr mrmp_instance, const Robot *r);
 
    virtual bool isValid(const ob::State *state) const;
private:
    const ob::SpaceInformation *si_;
    InstancePtr mrmp_instance_;
    const Robot *robot_; 
};
