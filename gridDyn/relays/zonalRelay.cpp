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

#include "zonalRelay.h"
#include "gridCondition.h"
#include "fileReaders.h"
#include "comms/gridCommunicator.h"
#include "comms/relayMessage.h"
#include "eventQueue.h"
#include "gridEvent.h"
#include "gridCoreTemplates.h"
#include "stringOps.h"


#include <boost/format.hpp>

zonalRelay::zonalRelay (const std::string&objName) : gridRelay (objName)
{
  opFlags.set (continuous_flag);
}

gridCoreObject *zonalRelay::clone (gridCoreObject *obj) const
{
  zonalRelay *nobj = cloneBase<zonalRelay, gridRelay> (this, obj);
  if (!(nobj))
    {
      return obj;
    }


  nobj->m_zones = m_zones;
  nobj->m_terminal = m_terminal;
  nobj->m_zoneLevels = m_zoneLevels;
  nobj->m_zoneDelays = m_zoneDelays;
  nobj->m_resetMargin = m_resetMargin;

  nobj->m_condition_level = m_condition_level;
  return nobj;
}

int zonalRelay::setFlag (const std::string &flag, bool val)
{
  int out = PARAMETER_FOUND;
  if (flag == "nondirectional")
    {
      opFlags.set (nondirectional_flag,val);
    }
  else
    {
      out = gridRelay::setFlag (flag, val);
    }
  return out;
}
/*
std::string commDestName;
std::uint64_t commDestId=0;
std::string commType;
*/
int zonalRelay::set (const std::string &param,  const std::string &val)
{
  int out = PARAMETER_FOUND;
  if (param == "levels")
    {
      auto dvals = splitline (val);
      set ("zones", dvals.size ());
      auto zL = m_zoneLevels.begin ();
      for (auto ld : dvals)
        {
          *zL = std::stod (ld);
          ++zL;
        }
    }
  else if (param == "delay")
    {
      auto dvals = splitline (val);
      if (dvals.size () != m_zoneDelays.size ())
        {
          out = INVALID_PARAMETER_VALUE;
        }
      auto zL = m_zoneDelays.begin ();
      for (auto ld : dvals)
        {
          *zL = std::stod (ld);
          ++zL;
        }
    }
  else
    {
      out = gridRelay::set (param, val);
    }
  return out;
}

int zonalRelay::set (const std::string &param, double val, gridUnits::units_t unitType)
{
  int out = PARAMETER_FOUND;
  index_t zn = 0;
  if (param == "zones")
    {
      m_zones = static_cast<count_t> (val);
      if (m_zones > m_zoneLevels.size ())
        {
          for (auto kk = m_zoneLevels.size (); kk < m_zones; ++kk)
            {
              if (kk == 0)
                {
                  m_zoneLevels.push_back (0.8);
                  m_zoneDelays.push_back (0);
                }
              else
                {
                  m_zoneLevels.push_back (m_zoneLevels[kk - 1] + 0.7);
                  m_zoneDelays.push_back (m_zoneDelays[kk - 1] + 1.0);
                }
            }

        }
      else
        {
          m_zoneLevels.resize (m_zones);
          m_zoneDelays.resize (m_zones);
        }
    }
  else if ((param == "terminal") || (param == "side"))
    {
      m_terminal = static_cast<index_t> (val);
    }
  else if ((param == "resetmargin") || (param == "margin"))
    {
      m_resetMargin = val;
    }
  else if (param == "autoname")
    {
      autoName = static_cast<int> (val);
    }
  else if (param.compare (0,5,"level") == 0)
    {
      if (param.size () == 6)
        {
          zn = (isdigit (param[5])) ? param[5] - '0' : 0;
        }
      else
        {
          zn = 0;
        }

      if (zn >= m_zones)
        {
          set ("zones", zn);

        }
      if (m_zoneLevels.size () < zn + 1)
        {
          m_zoneLevels.resize (zn + 1);

        }
      m_zoneLevels[zn] = val;
    }
  else if (param.compare (0, 5, "delay") == 0)
    {
      if (param.size () == 6)
        {
          zn = (isdigit (param[5])) ? param[5] - '0' : 0;
        }
      else
        {
          zn = 0;
        }

      if (zn >= m_zones)
        {
          set ("zones", zn);

        }
      if (m_zoneDelays.size () < zn + 1)
        {
          m_zoneDelays.resize (zn + 1);
        }
      m_zoneDelays[zn] = val;

    }
  else
    {
      out = gridRelay::set (param, val, unitType);
    }
  return out;
}


double zonalRelay::get (const std::string &param, gridUnits::units_t unitType) const
{
  double val = kNullVal;
  if (param == "condition")
    {
      val = kNullVal;
    }
  else
    {
      val = gridRelay::get (param, unitType);
    }
  return val;
}

void zonalRelay::dynObjectInitializeA (double time0, unsigned long flags)
{
  if (autoName > 0)
    {
      std::string newName = generateAutoName (autoName);
      if (!newName.empty ())
        {
          if (newName != name)
            {
              name = newName;
              alert (this, OBJECT_NAME_CHANGE);
            }
        }
      commName = name;
    }

  double baseImpedance = m_sourceObject->get ("impedance");
  for (index_t kk = 0; kk < m_zones; ++kk)
    {
      if (opFlags[nondirectional_flag])
        {
          add (make_condition ("abs(admittance" + std::to_string (m_terminal) + ")", ">=", 1.0 / (m_zoneLevels[kk] * baseImpedance), m_sourceObject));

        }
      else
        {
          add (make_condition ("admittance" + std::to_string (m_terminal), ">=", 1.0 / (m_zoneLevels[kk] * baseImpedance), m_sourceObject));
        }
      setResetMargin (kk, m_resetMargin * 1.0 / (m_zoneLevels[kk] * baseImpedance));
    }

  auto ge = std::make_shared<gridEvent> ();

  ge->field = "switch" + std::to_string (m_terminal);
  ge->value = 1;
  ge->setTarget (m_sinkObject);

  add (ge);
  for (index_t kk = 0; kk < m_zones; ++kk)
    {
      setActionTrigger (kk, 0, m_zoneDelays[kk]);
    }


  if (opFlags[use_commLink])
    {

      if (commDestName.substr (0, 4) == "auto")
        {
          if (commDestName.length () == 6)
            {
              int code = 0;
              try
                {
                  code = std::stoi (commDestName.substr (5, 1));
                }
              catch (std::invalid_argument)
                {
                  code = 0;
                }

              std::string newName = generateAutoName (code);
              if (!newName.empty ())
                {
                  commDestName = newName;
                }
            }
        }
    }
  return gridRelay::dynObjectInitializeA (time0,flags);
}


void zonalRelay::actionTaken (index_t ActionNum, index_t conditionNum, change_code /*actionReturn*/, double /*actionTime*/)
{
  LOG_NORMAL ((boost::format ("condition %d action %d taken terminal %d") %  conditionNum % ActionNum % m_terminal).str ());

  if (opFlags.test (use_commLink))
    {
      auto P = std::make_shared<relayMessage> (relayMessage::BREAKER_TRIP_EVENT);
      if (ActionNum == 0)
        {
          if (commDestName.empty ())
            {
              commLink->transmit (commDestId, P);
            }
          else
            {
              commLink->transmit (commDestName, P);
            }
        }
    }
  for (index_t kk = conditionNum + 1; kk < m_zones; ++kk)
    {
      setConditionState (kk, condition_states::disabled);
    }
  if (conditionNum < m_condition_level)
    {
      m_condition_level = conditionNum;
    }
}

void zonalRelay::conditionTriggered (index_t conditionNum, double /*triggerTime*/)
{
  LOG_NORMAL ((boost::format ("condition %d triggered terminal %d") % conditionNum % m_terminal).str ());
  if (conditionNum < m_condition_level)
    {
      m_condition_level = conditionNum;
    }
  if (opFlags.test (use_commLink))
    {
      if (conditionNum > m_condition_level)
        {
          return;
        }
      auto P = std::make_shared<relayMessage> ();
      //std::cout << "GridDyn conditionTriggered(), conditionNum = " << conditionNum << '\n';
      if (conditionNum == 0)
        {
          //std::cout << "GridDyn setting relay message type to LOCAL_FAULT_EVENT" << '\n';
          P->setMessageType (relayMessage::LOCAL_FAULT_EVENT);
        }
      else
        {
          //std::cout << "GridDyn setting relay message type to REMOTE_FAULT_EVENT" << '\n';
          P->setMessageType (relayMessage::REMOTE_FAULT_EVENT);
        }
      if (commDestName.empty ())
        {
          commLink->transmit (commDestId, P);
        }
      else
        {
          commLink->transmit (commDestName, P);
        }
    }

}

void zonalRelay::conditionCleared (index_t conditionNum, double /*triggerTime*/)
{
  LOG_NORMAL ((boost::format ("condition %d cleared terminal %d ") % conditionNum  % m_terminal).str ());
  for (index_t kk = 0; kk < m_zones; ++kk)
    {
      if (getConditionStatus (kk) == condition_states::active)
        {
          m_condition_level = kk + 1;
        }
      else
        {
          return;
        }
    }
  if (opFlags[use_commLink])
    {
      auto P = std::make_shared<relayMessage> ();
      if (conditionNum == 0)
        {
          P->setMessageType (relayMessage::LOCAL_FAULT_CLEARED);
        }
      else
        {
          P->setMessageType (relayMessage::REMOTE_FAULT_CLEARED);
        }
      if (commDestName.empty ())
        {
          commLink->transmit (commDestId, P);
        }
      else
        {
          commLink->transmit (commDestName, P);
        }
    }
}



void zonalRelay::receiveMessage (std::uint64_t /*sourceID*/, std::shared_ptr<commMessage> message)
{
  switch (message->getMessageType ())
    {
    case relayMessage::BREAKER_TRIP_COMMAND:
      triggerAction (0);
      break;
    case relayMessage::BREAKER_CLOSE_COMMAND:
      if (m_sinkObject)
        {
          m_sinkObject->set ("switch" + std::to_string (m_terminal), 0);
        }
      break;
    case relayMessage::BREAKER_OOS_COMMAND:
      for (unsigned int kk = 0; kk < m_zones; ++kk)
        {
          setConditionState (kk, condition_states::disabled);
        }
      break;
    default:
      {
        //assert (false);
      }
    }

}

std::string zonalRelay::generateAutoName (int code)
{
  std::string autoname = "";
  auto b1 = m_sourceObject->getSubObject ("bus", 1);
  auto b2 = m_sourceObject->getSubObject ("bus", 2);

  switch (code)
    {
    case 1:
      if (m_terminal == 1)
        {
          autoname = b1->getName () + '_' + b2->getName ();
        }
      else
        {
          autoname = b2->getName () + '_' + b1->getName ();
        }
      break;
    case 2:
      if (m_terminal == 1)
        {
          autoname = std::to_string (b1->getUserID ()) + '_' + std::to_string (b2->getUserID ());
        }
      else
        {
          autoname = std::to_string (b2->getUserID ()) + '_' + std::to_string (b1->getUserID ());
        }
      break;
    default:
      ;
      //do nothing
    }
  //check if there are multiple lines in parallel
  if (!autoname.empty ())
    {
      auto ri = m_sourceObject->getName ().rbegin ();
      if (*(ri + 1) == '_')
        {
          if ((*ri >= 'a') && (*ri <= 'z'))
            {
              autoname += '_' + (*ri);
            }
        }
    }
  return autoname;
}
