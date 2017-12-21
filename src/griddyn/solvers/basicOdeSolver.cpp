/*
* LLNS Copyright Start
* Copyright (c) 2017, Lawrence Livermore National Security
* This work was performed under the auspices of the U.S. Department
* of Energy by Lawrence Livermore National Laboratory in part under
* Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
* Produced at the Lawrence Livermore National Laboratory.
* All rights reserved.
* For details, see the LICENSE file.
* LLNS Copyright End
*/

#include "basicOdeSolver.h"

#include "gridDynSimulation.h"
#include "utilities/vectorOps.hpp"
#include <algorithm>
#include <cmath>

namespace griddyn
{
namespace solvers
{
basicOdeSolver::basicOdeSolver (const std::string &objName) : solverInterface (objName)
{
    mode.dynamic = true;
    mode.differential = true;
    mode.algebraic = false;
}
basicOdeSolver::basicOdeSolver (gridDynSimulation *gds, const solverMode &sMode) : solverInterface (gds, sMode) {}
std::unique_ptr<solverInterface> basicOdeSolver::clone (bool fullCopy) const
{
	std::unique_ptr<solverInterface> si = std::make_unique<basicOdeSolver>();
	basicOdeSolver::cloneTo(si.get(),fullCopy);
	return si;
}

void basicOdeSolver::cloneTo(solverInterface *si, bool fullCopy) const
{
	solverInterface::cloneTo(si, fullCopy);
	auto bos = dynamic_cast<basicOdeSolver *>(si);
	if (bos == nullptr)
	{
		return;
	}
	bos->deltaT = deltaT;
}

double *basicOdeSolver::state_data () noexcept { return state.data (); }
double *basicOdeSolver::deriv_data () noexcept { return deriv.data (); }
double *basicOdeSolver::type_data () noexcept { return type.data (); }
const double *basicOdeSolver::state_data () const noexcept { return state.data (); }
const double *basicOdeSolver::deriv_data () const noexcept { return deriv.data (); }
const double *basicOdeSolver::type_data () const noexcept { return type.data (); }
void basicOdeSolver::allocate (count_t stateCount, count_t numRoots)
{
    // load the vectors
    if (stateCount != svsize)
    {
        state.resize (stateCount);
        deriv.resize (stateCount);
        state2.resize (stateCount);
        svsize = stateCount;
        flags.reset (initialized_flag);
        flags.set (allocated_flag);
        rootsfound.resize (numRoots);
    }
}

void basicOdeSolver::initialize (coreTime t0)
{
    if (!flags[allocated_flag])
    {
        throw (InvalidSolverOperation (-2));
    }
    flags.set (initialized_flag);
    solverCallCount = 0;
    solveTime = t0;
}

double basicOdeSolver::get (const std::string &param) const
{
    if (param == "deltat")
    {
        return deltaT;
    }
    return solverInterface::get (param);
}
void basicOdeSolver::set (const std::string &param, const std::string &val)
{
    if (param[0] == '#')
    {
    }
    else
    {
        solverInterface::set (param, val);
    }
}
void basicOdeSolver::set (const std::string &param, double val)
{
    if ((param == "delta") || (param == "deltat") || (param == "step") || (param == "steptime"))
    {
        deltaT = val;
    }
    else
    {
        solverInterface::set (param, val);
    }
}

int basicOdeSolver::solve (coreTime tStop, coreTime &tReturn, step_mode stepMode)
{
    if (solveTime == tStop)
    {
        tReturn = tStop;
        return FUNCTION_EXECUTION_SUCCESS;
    }
    coreTime Tstep = (std::min) (deltaT, tStop - solveTime);
    if (mode.pairedOffsetIndex != kNullLocation)
    {
        int ret = m_gds->dynAlgebraicSolve (solveTime, state.data (), deriv.data (), mode);
        if (ret < FUNCTION_EXECUTION_SUCCESS)
        {
            return ret;
        }
    }
    m_gds->derivativeFunction (solveTime, state.data (), deriv.data (), mode);
    std::transform (state.begin (), state.end (), deriv.begin (), state.begin (),
                    [Tstep](double a, double b) { return fma (Tstep, b, a); });
    solveTime += Tstep;
    // if we are in single step mode don't go into the loop
    if (stepMode == step_mode::normal)
    {
        while (solveTime < tStop)
        {
            Tstep = (std::min) (deltaT, tStop - solveTime);
            if (mode.pairedOffsetIndex != kNullLocation)
            {
                int ret = m_gds->dynAlgebraicSolve (solveTime, state.data (), deriv.data (), mode);
                if (ret < FUNCTION_EXECUTION_SUCCESS)
                {
                    return ret;
                }
            }
            m_gds->derivativeFunction (solveTime, state.data (), deriv.data (), mode);
            std::transform (state.begin (), state.end (), deriv.begin (), state.begin (),
                            [Tstep](double a, double b) { return fma (Tstep, b, a); });
            solveTime += Tstep;
        }
    }
    tReturn = solveTime;
    return FUNCTION_EXECUTION_SUCCESS;
}

}  // namespace solvers
}  // namespace griddyn