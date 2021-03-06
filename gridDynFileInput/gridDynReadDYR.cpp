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

#include "gridDynFileInput.h"
#include "objectFactory.h"
#include "submodels/gridDynGenModel.h"
#include "submodels/gridDynExciter.h"
#include "submodels/gridDynGovernor.h"
#include "generators/gridDynGenerator.h"
#include "gridBus.h"

#include "stringOps.h"

#include <fstream>
#include <iostream>
#include <cstdio>

static std::shared_ptr<coreObjectFactory> cof = coreObjectFactory::instance();

void loadGENROU(gridCoreObject *parentObject, stringVec &tokens);
void loadESDC1A(gridCoreObject *parentObject, stringVec &tokens);
void loadTGOV1(gridCoreObject *parentObject, stringVec &tokens);
void loadEXDC2(gridCoreObject *parentObject, stringVec &tokens);

void loadDYR(gridCoreObject *parentObject,const std::string &filename,const basicReaderInfo &)
{
  std::ifstream file(filename.c_str(), std::ios::in);
  std::string line,line2;  //line storage
  std::string temp1;  //temporary storage for substrings

  if (!(file.is_open()))
  {
    parentObject->log(parentObject,GD_ERROR_PRINT, "Unable to open file " + filename );
    //	return;
  }
  while (std::getline(file, line))
  {
    trimString(line);
    if (line.empty())
    {
      continue;
    }
    while (line.back() != '/')
    {
    if (std::getline(file, line2))
      {
      trimString(line2);
      line += ' ' + line2;
      }
    else
      {
      break;
      }
    }
    auto lineTokens = splitline(line," \t\n,",delimiter_compression::on);
    //get rid of the '/' at the end of the last string
    auto lstr = lineTokens.back();
    lineTokens.pop_back();
    lstr = lstr.substr(0, lstr.size() - 1);
    if (!lstr.empty())
    {
      lineTokens.push_back(lstr);
    }
    auto type = lineTokens[1];
	trimString(type);
    if (type == "'GENROU'")
    {
      loadGENROU(parentObject, lineTokens);
    }
    else if (type == "'ESDC1A'")
    {
      loadESDC1A(parentObject, lineTokens);
    }
    else if (type == "'EXDC2'")
    {
      loadESDC1A(parentObject, lineTokens);
    }
    else if (type == "'TGOV1'")
    {
      loadTGOV1(parentObject, lineTokens);
    }
    else
    {
      std::cout << "unknown object type " << type << '\n';
    }
  }
}


  void loadGENROU(gridCoreObject *parentObject, stringVec &tokens)
  {
    int id = std::stoi(tokens[0]);
    gridBus *bus = static_cast<gridBus *>(parentObject->findByUserID("bus", id));
    id = std::stoi(tokens[2]);
    gridDynGenerator *gen = bus->getGen(id - 1);

    auto params = str2vector(tokens,kNullVal);

    gridDynGenModel *sm = static_cast<gridDynGenModel *>(cof->createObject("genmodel", "6"));
    sm->set("tdop", params[3]);
    sm->set("tdopp", params[4]);
    sm->set("tqop", params[5]);
    sm->set("tqopp", params[6]);
    sm->set("h", params[7]);
    sm->set("d", params[8]);
    sm->set("xd", params[9]);
    sm->set("xq", params[10]);
    sm->set("xdp", params[11]);
    sm->set("xqp", params[12]);
    sm->set("xdpp", params[13]);
    sm->set("xqpp", params[13]);
    sm->set("xl", params[14]);
    sm->set("s1", params[15]);
    sm->set("s12", params[16]);
    
    gen->add(sm);

  }

  void loadESDC1A(gridCoreObject *parentObject, stringVec &tokens)
  {
    int id = std::stoi(tokens[0]);
    gridBus *bus = static_cast<gridBus *>(parentObject->findByUserID("bus", id));
    id = std::stoi(tokens[2]);
    gridDynGenerator *gen = bus->getGen(id - 1);

    auto params = str2vector(tokens, kNullVal);
    gridDynExciter *sm;
    if (params[6] > 0.0)//dc1a model must have tb>0 otherwise revert to type1
    {
      sm = static_cast<gridDynExciter *>(cof->createObject("exciter", "dc1a"));
    }
    else
    {
      sm = static_cast<gridDynExciter *>(cof->createObject("exciter", "type1"));
    }
    //TODO:: TR not implmented yet, no voltage compensation implemented
    //sm->set("tr", params[3]);
    sm->set("ka", params[4]);
    sm->set("ta", params[5]);
    if (params[6] > 0) 
    {
      sm->set("tb", params[6]);
      sm->set("tc", params[7]);
    }
    sm->set("vrmax", params[8]);
    sm->set("vrmin", params[9]);
    sm->set("ke", params[10]);
    sm->set("te", params[11]);
    sm->set("kf", params[12]);
    sm->set("tf", params[13]);
    //TODO I need to compute the saturation coeeficients to translate appropriately

    gen->add(sm);

  }

  void loadEXDC2(gridCoreObject *parentObject, stringVec &tokens)
  {
    int id = std::stoi(tokens[0]);
    gridBus *bus = static_cast<gridBus *>(parentObject->findByUserID("bus", id));
    id = std::stoi(tokens[2]);
    gridDynGenerator *gen = bus->getGen(id - 1);

    auto params = str2vector(tokens, kNullVal);

    gridDynExciter *sm = static_cast<gridDynExciter *>(cof->createObject("exciter", "dc2a"));
    //TODO:: TR not implmented yet, no voltage compensation implemented
    //sm->set("tr", params[3]);
    sm->set("ka", params[4]);
    sm->set("ta", params[5]);
    sm->set("tb", params[6]);
    sm->set("tc", params[7]);
    sm->set("vrmax", params[8]);
    sm->set("vrmin", params[9]);
    sm->set("ke", params[10]);
    sm->set("te", params[11]);
    sm->set("kf", params[12]);
    sm->set("tf", params[13]);
    //TODO I need to compute the saturation coefficients to translate appropriately

    gen->add(sm);

  }

  void loadTGOV1(gridCoreObject *parentObject, stringVec &tokens)
  {
    int id = std::stoi(tokens[0]);
    gridBus *bus = static_cast<gridBus *>(parentObject->findByUserID("bus", id));
    id = std::stoi(tokens[2]);
    gridDynGenerator *gen = bus->getGen(id - 1);

    auto params = str2vector(tokens, kNullVal);

    gridDynGovernor *sm = static_cast<gridDynGovernor *>(cof->createObject("governor", "tgov1"));
    //TODO:: TR not implmented yet, no voltage compensation implemented
    //sm->set("tr", params[3]);
    sm->set("r", params[3]);
    sm->set("t1", params[4]);
    sm->set("pmax", params[5]);
    sm->set("pmin", params[6]);
    sm->set("t2", params[6]);
    sm->set("t3", params[7]);
    sm->set("dt", params[8]);
    

    gen->add(sm);

  }