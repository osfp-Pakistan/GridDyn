/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
  * LLNS Copyright Start
 * Copyright (c) 2016, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department
 * of Energy by Lawrence Livermore National Laboratory in part under
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
*/

// headers
#include "gridGenOpt.h"
#include "generators/gridDynGenerator.h"
#include "gridBusOpt.h"
#include "optObjectFactory.h"
#include "vectorOps.hpp"
#include "vectData.h"
#include "arrayData.h"
#include "gridCoreTemplates.h"



#include <cmath>
#include <utility>



static optObjectFactory<gridGenOpt, gridDynGenerator> opgen ("basic", "gen",0,true);

using namespace gridUnits;

gridGenOpt::gridGenOpt (const std::string &objName) : gridOptObject (objName)
{

}

gridGenOpt::gridGenOpt (gridCoreObject *obj, const std::string &objName) : gridOptObject (objName),gen (dynamic_cast<gridDynGenerator *> (obj))
{

  if (gen)
    {
      if (name.empty ())
        {
          name = gen->getName ();
        }
      id = gen->getUserID ();
    }
}

gridCoreObject *gridGenOpt::clone (gridCoreObject *obj) const
{
  gridGenOpt *nobj = cloneBase<gridGenOpt, gridOptObject> (this, obj);
  if (nobj == nullptr)
    {
      return obj;
    }
  nobj->m_heatRate = m_heatRate;
  nobj->Pcoeff = Pcoeff;
  nobj->Qcoeff = Qcoeff;
  nobj->m_penaltyCost = m_penaltyCost;
  nobj->m_fuelCost = m_fuelCost;
  nobj->m_forecast = m_forecast;
  nobj->m_Pmax = m_Pmax;
  nobj->m_Pmin = m_Pmin;


  return nobj;
}

int gridGenOpt::add (gridCoreObject *obj)
{
  if (dynamic_cast<gridDynGenerator *> (obj))
    {
      gen = static_cast<gridDynGenerator *> (obj);
      name = gen->getName ();
      id = gen->getUserID ();
      return OBJECT_ADD_SUCCESS;
    }
  return OBJECT_NOT_RECOGNIZED;
}

void gridGenOpt::objectInitializeA (unsigned long /*flags*/)
{
  bus = static_cast<gridBusOpt *> (parent->find ("bus"));

}

void gridGenOpt::loadSizes (const optimMode &oMode)
{
  optimOffsets *oo = offsets.getOffsets (oMode);
  oo->reset ();
  switch (oMode.flowMode)
    {
    case flowModel_t::none:
    case flowModel_t::transport:
    case flowModel_t::dc:
      oo->local.genSize = 1;
      break;
    case flowModel_t::ac:
      oo->local.genSize = 1;
      oo->local.qSize = 1;
      break;
    }
  oo->localLoad (true);
}




void gridGenOpt::setValues (const optimData *, const optimMode & /*oMode*/)
{
}

//for saving the state
void gridGenOpt::guess (double /*ttime*/, double /*val*/[], const optimMode & /*oMode*/)
{
  //optimOffsets *oo = offsets.getOffsets (oMode);

}

void gridGenOpt::getVariableType (double /*sdata*/[], const optimMode &)
{

}

void gridGenOpt::getTols (double /*tols*/[], const optimMode &)
{
}


void gridGenOpt::valueBounds (double ttime, double upperLimit[], double lowerLimit[], const optimMode &oMode)
{
  optimOffsets *oo = offsets.getOffsets (oMode);
  double Pup,Pdown;
  if (optFlags.test (limit_override))
    {
      if (m_Pmax < kHalfBigNum)
        {
          Pup = m_Pmax;
        }
      else
        {
          Pup = gen->getPmax (ttime);
        }
      if (m_Pmin > -kHalfBigNum)
        {
          Pdown = m_Pmin;
        }
      else
        {
          Pdown = gen->getPmin (ttime);
        }
    }
  else
    {
      Pup = gen->getPmax (ttime);
      Pdown = gen->getPmin (ttime);
    }
  upperLimit[oo->gOffset] = Pup;
  lowerLimit[oo->gOffset] = Pdown;
  if (isAC (oMode))
    {
      double Qup = gen->getQmax (ttime);
      double Qdown = gen->getQmin (ttime);
      upperLimit[oo->qOffset] = Qup;
      lowerLimit[oo->qOffset] = Qdown;
    }

}

void gridGenOpt::linearObj (const optimData *, vectData *linObj, const optimMode &oMode)
{
  optimOffsets *oo = offsets.getOffsets (oMode);
  if (optFlags.test (piecewise_linear_cost))
    {

    }
  else
    {
      linObj->assign (0,Pcoeff[0] * oMode.period);
      linObj->assign (oo->gOffset, Pcoeff[1] * oMode.period);
      if ((!(Qcoeff.empty ())) && (isAC (oMode)))
        {
          linObj->assign (0, Qcoeff[0] * oMode.period);
          linObj->assign (oo->qOffset, Qcoeff[1] * oMode.period);
        }
    }
}
void gridGenOpt::quadraticObj (const optimData *, vectData *linObj, vectData * quadObj, const optimMode &oMode)
{
  optimOffsets *oo = offsets.getOffsets (oMode);
  if (optFlags[piecewise_linear_cost])
    {

    }
  else
    {
      linObj->assign (0, Pcoeff[0] * oMode.period);
      linObj->assign (oo->gOffset, Pcoeff[1] * oMode.period);
      if (Pcoeff.size () >= 2)
        {
          quadObj->assign (oo->gOffset, Pcoeff[2] * oMode.period);
        }
      if ((!(Qcoeff.empty ())) && (isAC (oMode)))
        {
          linObj->assign (0, Qcoeff[0] * oMode.period);
          linObj->assign (oo->qOffset, Qcoeff[1] * oMode.period);
          if (Qcoeff.size () >= 2)
            {
              quadObj->assign (oo->qOffset, Qcoeff[2] * oMode.period);
            }
        }
    }
}

double gridGenOpt::objValue (const optimData *oD, const optimMode &oMode)
{
  double cost = 0;
  optimOffsets *oo = offsets.getOffsets (oMode);
  double P = oD->val[oo->gOffset];
  if (optFlags[piecewise_linear_cost])
    {

    }
  else
    {
      int kk = 0;
      int kmax;
      switch (oMode.linMode)
        {
        case linearityMode_t::linear:
          kmax = 1;
          break;
        case linearityMode_t::quadratic:
          kmax = 2;
          break;
        default:
          kmax = kBigINT;
        }
      for (auto pc:Pcoeff)
        {
          cost += pc * pow (P, kk) * oMode.period;
          ++kk;
          if (kk > kmax)
            {
              break;
            }
        }
      if ((!(Qcoeff.empty ()))&&(isAC (oMode)))
        {
          double Q = oD->val[oo->qOffset];
          kk = 0;
          for (auto pc : Qcoeff)
            {
              cost += pc * pow (Q, kk) * oMode.period;
              ++kk;
              if (kk > kmax)
                {
                  break;
                }
            }
        }
    }
  return cost;
}

void gridGenOpt::derivative (const optimData *oD, double deriv[], const optimMode &oMode)
{
  optimOffsets *oo = offsets.getOffsets (oMode);
  double P = oD->val[oo->gOffset];
  double temp;
  if (optFlags[piecewise_linear_cost])
    {

    }
  else
    {

      size_t kk = 0;
      size_t kmax;
      size_t klim;
      switch (oMode.linMode)
        {
        case linearityMode_t::linear:
          kmax = 1;
          break;
        case linearityMode_t::quadratic:
          kmax = 2;
          break;
        default:
          kmax = kBigINT;
        }
      klim = std::max (kmax,Pcoeff.size ());
      temp = 0;
      for (kk = 1; kk < klim; ++kk)
        {
          temp += static_cast<double> (kk) * Pcoeff[kk] * pow (P, kk - 1) * oMode.period;
        }
      deriv[oo->gOffset] = temp;
      if ((!(Qcoeff.empty ())) && (isAC (oMode)))
        {
          double Q = oD->val[oo->qOffset];
          klim = std::max (kmax, Qcoeff.size ());
          temp = 0;
          for (kk = 1; kk < klim; ++kk)
            {
              temp += static_cast<double> (kk) * Qcoeff[kk] * pow (Q, kk - 1) * oMode.period;
            }
          deriv[oo->qOffset] = temp;
        }
    }
}
void gridGenOpt::jacobianElements (const optimData *oD, arrayData<double> *ad, const optimMode &oMode)
{
  optimOffsets *oo = offsets.getOffsets (oMode);
  double P = oD->val[oo->gOffset];
  double temp;
  if (optFlags[piecewise_linear_cost])
    {

    }
  else
    {

      size_t kk = 0;
      size_t kmax;
      size_t klim;
      switch (oMode.linMode)
        {
        case linearityMode_t::linear:
          kmax = 1;
          break;
        case linearityMode_t::quadratic:
          kmax = 2;
          break;
        default:
          kmax = kBigINT;
        }
      klim = std::max (kmax, Pcoeff.size ());
      temp = 0;
      for (kk = 1; kk < klim; ++kk)
        {
          temp += static_cast<double> (kk) * Pcoeff[kk] * pow (P, kk - 1) * oMode.period;
        }
      if (temp != 0)
        {
          ad->assign (oo->gOffset,oo->gOffset, temp);
        }
      if ((!(Qcoeff.empty ())) && (isAC (oMode)))
        {
          double Q = oD->val[oo->qOffset];
          klim = std::max (kmax, Qcoeff.size ());
          temp = 0;
          for (kk = 1; kk < klim; ++kk)
            {
              temp += static_cast<double> (kk) * Qcoeff[kk] * pow (Q, kk - 1) * oMode.period;
            }
          if (temp != 0)
            {
              ad->assign (oo->qOffset, oo->qOffset, temp);
            }
        }
    }
}

void gridGenOpt::getConstraints (const optimData *, arrayData<double> * /*cons*/, double /*upperLimit*/[], double /*lowerLimit*/[], const optimMode &)
{

}

void gridGenOpt::constraintValue (const optimData *, double /*cVals*/ [], const optimMode &)
{

}

void gridGenOpt::constraintJacobianElements (const optimData *, arrayData<double> *, const optimMode &)
{

}

void gridGenOpt::getObjName (stringVec &objNames, const optimMode &oMode, const std::string &prefix)
{
  optimOffsets *oo = offsets.getOffsets (oMode);
  objNames[oo->gOffset] = prefix + name + ":PGen";
  if (isAC (oMode))
    {
      objNames[oo->gOffset] = prefix + name + ":QGen";
    }
}


// destructor
gridGenOpt::~gridGenOpt ()
{

}



// set properties
int gridGenOpt::set (const std::string &param,  const std::string &val)
{
  int out = PARAMETER_FOUND;
  if (param == "#")
    {

    }
  else
    {
      out = gridOptObject::set (param, val);
    }
  return out;
}

int gridGenOpt::set (const std::string &param, double val, units_t unitType)
{
  int out = PARAMETER_FOUND;
  unsigned long num = 0;

  if (param[0] == 'p')
    {
      try
        {
          num = std::stoul (param.substr (1));
          if (num > Pcoeff.size ())
            {
              Pcoeff.resize (num + 1);
            }
          Pcoeff[num] = val;
          return out;
        }
      catch (std::invalid_argument)
        {
          //if it doesn't work just let it pass through to the lower statements
        }

    }
  else if (param[0] == 'q')
    {
      try
        {

          num = std::stoul (param.substr (1));
          if (num > Qcoeff.size ())
            {
              Qcoeff.resize (num + 1);
            }
          Qcoeff[num] = val;
          return out;
        }
      catch (std::invalid_argument)
        {
          //if it doesn't work just let it pass through to the lower statements
        }
    }

  if (param == "heatrate")
    {
      m_heatRate = val;
    }
  else if ((param == "fuel")||(param == "fuelcost"))
    {
      m_fuelCost = val;
    }
  else if ((param == "constantp")||(param == "constant"))
    {
      if (Pcoeff.empty ())
        {
          Pcoeff.resize (1);
        }
      Pcoeff[0] = val;

    }
  else if ((param == "linearp")||(param == "linear"))
    {
      if (Pcoeff.size () < 2)
        {
          Pcoeff.resize (2);
        }
      Pcoeff[1] = unitConversion (val,unitType,CppuMWh,systemBasePower);
    }
  else if ((param == "quadraticp")||(param == "quadp")||(param == "quadratic")||(param == "quad"))
    {
      if (Pcoeff.size () < 3)
        {
          Pcoeff.resize (3);
        }
      Pcoeff[2] = unitConversion (val, unitType, CppuMW2h, systemBasePower);
    }
  else if (param == "constantq")
    {
      if (Qcoeff.empty ())
        {
          Qcoeff.resize (1);
        }
      Qcoeff[0] = val;
    }
  else if (param == "linearq")
    {
      if (Qcoeff.size () < 2)
        {
          Qcoeff.resize (2);
        }
      Qcoeff[1] = unitConversion (val, unitType, CppuMVARh, systemBasePower);
    }
  else if ((param == "quadraticq") || (param == "quadq"))
    {
      if (Qcoeff.size () < 3)
        {
          Qcoeff.resize (3);
        }
      Qcoeff[1] = unitConversion (val, unitType, CppuMVAR2h, systemBasePower);
    }
  else if ((param == "penalty_cost")||(param == "penalty"))
    {
      m_penaltyCost = unitConversion (val,unitType,CppuMWh,systemBasePower);
    }
  else if (param == "pmax")
    {
      m_Pmax = unitConversion (val, unitType, puMW, systemBasePower);
      optFlags.set (limit_override);
    }
  else if (param == "pmin")
    {
      m_Pmin = unitConversion (val, unitType, puMW, systemBasePower);
      optFlags.set (limit_override);
    }
  else if (param == "forecast")
    {
      m_forecast = unitConversion (val, unitType, puMW, systemBasePower);
    }
  else
    {
      out = gridOptObject::set (param, val, unitType);
    }


  return out;
}




double gridGenOpt::get (const std::string &param, gridUnits::units_t unitType) const
{
  double val = kNullVal;
  if (param == "#")
    {

    }
  else
    {
      val = gridOptObject::get (param,unitType);
    }
  return val;
}


void gridGenOpt::loadCostCoeff (std::vector<double> coeff, int mode)
{
  if (mode == 0)
    {
      Pcoeff = coeff;
    }
  else
    {
      Qcoeff = coeff;
    }
}

gridOptObject *gridGenOpt::getBus (index_t /*index*/) const
{
  return bus;
}

gridOptObject *gridGenOpt::getArea (index_t index) const
{
  return bus->getArea (index);
}




