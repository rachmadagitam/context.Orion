#ifndef SRC_LIB_APITYPESV2_SUBSCRIPTION_H
#define SRC_LIB_APITYPESV2_SUBSCRIPTION_H

/*
*
* Copyright 2015 Telefonica Investigacion y Desarrollo, S.A.U
*
* This file is part of Orion Context Broker.
*
* Orion Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* iot_support at tid dot es
*
* Author: Orion dev team
*/
#include <string>
#include <vector>

#include "ngsi/Duration.h"
#include "ngsi/Throttling.h"
#include "apiTypesV2/HttpInfo.h"

namespace ngsiv2
{

struct EntID
{
  std::string id;
  std::string idPattern;
  std::string type;
  std::string toJson();
};

struct Notification
{
  std::vector<std::string> attributes;  
  long long                timesSent;
  long long                lastNotification;
  HttpInfo                 httpInfo;
  std::string              toJson();
};



struct Condition
{
  std::vector<std::string> attributes;
  struct {
    std::string q;
    std::string geometry;
    std::string coords;
    std::string georel;
  }                        expression;
  std::string toJson();
};



struct Subject
{
  std::vector<EntID> entities;
  Condition          condition;
  std::string        toJson();
};


struct Subscription
{
  std::string  id;
  std::string  description;
  Subject      subject;
  long long    expires;
  std::string  status;
  Notification notification;
  long long    throttling;
  std::string  toJson();
};



} // end namespace

#endif // SRC_LIB_APITYPESV2_SUBSCRIPTION_H
