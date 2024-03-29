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

#include <algorithm>

#include "rapidjson/document.h"

#include "alarmMgr/alarmMgr.h"
#include "common/globals.h"
#include "common/RenderFormat.h"
#include "rest/ConnectionInfo.h"
#include "rest/OrionError.h"
#include "ngsi/ParseData.h"
#include "ngsi/Request.h"
#include "jsonParseV2/jsonParseTypeNames.h"
#include "jsonParseV2/jsonRequestTreat.h"
#include "jsonParseV2/parseSubscription.h"

using namespace rapidjson;



/* Prototypes */
static std::string parseAttributeList(ConnectionInfo* ciP, std::vector<std::string>* vec, const Value& attributes);
static std::string parseNotification(ConnectionInfo* ciP, SubscribeContextRequest* scrP, const Value& notification);
static std::string parseSubject(ConnectionInfo* ciP, SubscribeContextRequest* scrP, const Value& subject);
static std::string parseEntitiesVector(ConnectionInfo* ciP, EntityIdVector* eivP, const Value& entities);
static std::string parseNotifyConditionVector(ConnectionInfo* ciP, SubscribeContextRequest* scrP, const Value& condition);



/* ****************************************************************************
*
* parseSubscription -
*
*/
std::string parseSubscription(ConnectionInfo* ciP, ParseData* parseDataP, JsonDelayedRelease* releaseP, bool update)
{
  Document                  document;
  SubscribeContextRequest*  destination;

  document.Parse(ciP->payload);

  if (update)
  {
    destination     = &parseDataP->ucsr.res;
    releaseP->ucsrP = &parseDataP->ucsr.res;
  }
  else
  {
    destination    = &parseDataP->scr.res;
    releaseP->scrP = &parseDataP->scr.res;
  }

  if (document.HasParseError())
  {
    OrionError oe(SccBadRequest, "Errors found in incoming JSON buffer", ERROR_STRING_PARSERROR);
    alarmMgr.badInput(clientIp, "JSON parse error");
    return oe.render(ciP, "");
  }

  if (!document.IsObject())
  {
    OrionError oe(SccBadRequest, "Error parsing incoming JSON buffer", ERROR_STRING_PARSERROR);
    alarmMgr.badInput(clientIp, "JSON parse error");
    return oe.render(ciP, "");
  }

  if (document.ObjectEmpty())
  {
    //
    // Initially we used the method "Empty". As the broker crashed inside that method, some
    // research was made and "ObjectEmpty" was found. As the broker stopped crashing and complaints
    // about crashes with small docs and "Empty()" were found on the internet, we opted to use ObjectEmpty
    //
    OrionError oe(SccBadRequest, "empty payload");

    alarmMgr.badInput(clientIp, "empty payload");
    return oe.render(ciP, "");
  }


  // Description field
  destination->descriptionProvided = false;
  if (document.HasMember("description"))
  {
    const Value& description      = document["description"];

    if (!description.IsString())
    {
      OrionError oe(SccBadRequest, "description must be a string");

      alarmMgr.badInput(clientIp, "description must be a string");
      return oe.render(ciP, "");
    }

    std::string descriptionString = description.GetString();

    if (descriptionString.length() > MAX_DESCRIPTION_LENGTH)
    {
      OrionError oe(SccBadRequest, "max description length exceeded");

      alarmMgr.badInput(clientIp, "max description length exceeded");
      return oe.render(ciP, "");
    }

    destination->descriptionProvided = true;
    destination->description = descriptionString;
  }


  // Subject field
  if (document.HasMember("subject"))
  {
    const Value&  subject = document["subject"];
    std::string   r       = parseSubject(ciP, destination, subject);

    if (r != "")
    {
      return r;
    }
  }
  else if (!update)
  {
    OrionError oe(SccBadRequest, "no subject for subscription specified");

    alarmMgr.badInput(clientIp, "no subject specified");
    return oe.render(ciP, "");
  }


  // Notification field
  if (document.HasMember("notification"))
  {
    const Value&  notification = document["notification"];
    std::string   r            = parseNotification(ciP, destination, notification);

    if (r != "")
    {
      return r;
    }
  }
  else if (!update)
  {
    OrionError oe(SccBadRequest, "no notification for subscription specified");

    alarmMgr.badInput(clientIp, "no notification specified");
    return oe.render(ciP, "");
  }


  // Expires field
  if (document.HasMember("expires"))
  {
    const Value& expires = document["expires"];

    if (!expires.IsString())
    {
      OrionError oe(SccBadRequest, "expires is not a string");

      alarmMgr.badInput(clientIp, "expires is not a string");
      return oe.render(ciP, "");
    }

    int64_t eT = -1;

    if (expires.GetStringLength() == 0)
    {
        eT = PERMANENT_SUBS_DATETIME;
    }
    else
    {
      eT = parse8601Time(expires.GetString());
      if (eT == -1)
      {
        OrionError oe(SccBadRequest, "expires has an invalid format");

        alarmMgr.badInput(clientIp, "expires has an invalid format");
        return oe.render(ciP, "");
      }
    }

    destination->expires = eT;
  }
  else if (!update)
  {
    destination->expires = PERMANENT_SUBS_DATETIME;
  }


  // Status field
  if (document.HasMember("status"))
  {
    const Value& status = document["status"];

    if (!status.IsString())
    {
      OrionError oe(SccBadRequest, "status is not a string");

      alarmMgr.badInput(clientIp, "status is not a string");
      return oe.render(ciP, "");
    }

    std::string statusString = status.GetString();

    if ((statusString != "active") && (statusString != "inactive"))
    {
      OrionError oe(SccBadRequest, "status is not valid (it has to be either active or inactive)");

      alarmMgr.badInput(clientIp, "status is not valid (it has to be either active or inactive)");
      return oe.render(ciP, "");
    }

    destination->status = statusString;
  }


  // Throttling
  if (document.HasMember("throttling"))
  {
    const Value& throttling = document["throttling"];
    if (!throttling.IsInt64())
    {
      alarmMgr.badInput(clientIp, "throttling is not an int");
      OrionError oe(SccBadRequest, "throttling is not an int");

      return oe.render(ciP, "");
    }
    destination->throttling.seconds = throttling.GetInt64();
  }
  else if (!update) // throttling was not set and it is not update
  {
    destination->throttling.seconds = 0;
  }


  // attrsFormat field
  if (document.HasMember("attrsFormat"))
  {
    const Value& attrsFormat = document["attrsFormat"];

    if (!attrsFormat.IsString())
    {
      OrionError oe(SccBadRequest, "attrsFormat is not a string");

      alarmMgr.badInput(clientIp, "attrsFormat is not a string");
      return oe.render(ciP, "");
    }

    std::string   attrsFormatString = attrsFormat.GetString();
    RenderFormat  nFormat           = stringToRenderFormat(attrsFormatString, true);

    if (nFormat == NO_FORMAT)
    {
      const char*  details  = "invalid attrsFormat (accepted values: legacy, normalized, keyValues, values)";
      OrionError   oe(SccBadRequest, details);

      alarmMgr.badInput(clientIp, details);
      return oe.render(ciP, "");
    }

    destination->attrsFormat = nFormat;
  }
  else if (destination->attrsFormat == NO_FORMAT)
  {
    destination->attrsFormat = DEFAULT_RENDER_FORMAT;  // Default format for NGSIv2: normalized
  }

  return "OK";
}



/* ****************************************************************************
*
* parseSubject -
*
*/
static std::string parseSubject(ConnectionInfo* ciP, SubscribeContextRequest* scrP, const Value& subject)
{
  std::string r;

  if (!subject.IsObject())
  {
    OrionError oe(SccBadRequest, "subject is not an object");

    alarmMgr.badInput(clientIp, "subject is not an object");
    return oe.render(ciP, "");

  }
  // Entities
  if (!subject.HasMember("entities"))
  {
    OrionError oe(SccBadRequest, "no subject entities specified");

    alarmMgr.badInput(clientIp, "no subject entities specified");
    return oe.render(ciP, "");
  }

  r = parseEntitiesVector(ciP, &scrP->entityIdVector, subject["entities"] );
  if (r != "")
  {
    return r;
  }


  // Condition
  if (!subject.HasMember("condition"))
  {
    OrionError oe(SccBadRequest, "no subject condition specified");

    alarmMgr.badInput(clientIp, "no subject condition specified");
    return oe.render(ciP, "");
  }

  const Value& condition = subject["condition"];
  if (!condition.IsObject())
  {
    OrionError oe(SccBadRequest, "condition is not an object");

    alarmMgr.badInput(clientIp, "condition is not an object");
    return oe.render(ciP, "");
  }
  r = parseNotifyConditionVector(ciP, scrP, condition);

  return r;
}



/* ****************************************************************************
*
* parseEntitiesVector -
*
*/
static std::string parseEntitiesVector(ConnectionInfo* ciP, EntityIdVector* eivP, const Value& entities)
{
  if (!entities.IsArray())
  {
    alarmMgr.badInput(clientIp, "subject entities is not an array");
    OrionError oe(SccBadRequest, "subject entities is not an array");

    return oe.render(ciP, "");
  }

  for (Value::ConstValueIterator iter = entities.Begin(); iter != entities.End(); ++iter)
  {
    if (!iter->IsObject())
    {
      alarmMgr.badInput(clientIp, "subject entities element is not an object");
      OrionError oe(SccBadRequest, "subject entities element is not an object");

      return oe.render(ciP, "");
    }
    if (!iter->HasMember("id") && !iter->HasMember("idPattern") && !iter->HasMember("type"))
    {
      alarmMgr.badInput(clientIp, "subject entities element has not id/idPattern nor type");
      OrionError oe(SccBadRequest, "subject entities element has not id/idPattern");

      return oe.render(ciP, "");
    }

    if (iter->HasMember("id") && iter->HasMember("idPattern"))
    {
      alarmMgr.badInput(clientIp, "subject entities element has id and idPattern");
      OrionError oe(SccBadRequest, "subject entities element has id and idPattern");

      return oe.render(ciP, "");
    }


    std::string  id;
    std::string  type;
    std::string  isPattern  = "false";

    if (iter->HasMember("id"))
    {
      if ((*iter)["id"].IsString())
      {
        id = (*iter)["id"].GetString();
      }
      else
      {
        alarmMgr.badInput(clientIp, "subject entities element id is not a string");
        OrionError oe(SccBadRequest, "subject entities element id is not a string");

        return oe.render(ciP, "");
      }
    }
    if (iter->HasMember("idPattern"))
    {
      if ((*iter)["idPattern"].IsString())
      {
        id = (*iter)["idPattern"].GetString();
      }
      else
      {
        alarmMgr.badInput(clientIp, "subject entities element idPattern is not a string");
        OrionError oe(SccBadRequest, "subject entities element idPattern is not a string");

        return oe.render(ciP, "");
      }

      isPattern = "true";
    }
    if (id == "")  // Only type was provided
    {
      id        = ".*";
      isPattern = "true";
    }


    if (iter->HasMember("type"))
    {
      if ((*iter)["type"].IsString())
      {
        type = (*iter)["type"].GetString();
      }
      else
      {
        alarmMgr.badInput(clientIp, "subject entities element type is not a string");
        OrionError oe(SccBadRequest, "subject entities element type is not a string");

        return oe.render(ciP, "");
      }
    }

    EntityId*  eiP = new EntityId(id, type, isPattern);

    eivP->push_back_if_absent(eiP);
  }

  return "";
}



/* ****************************************************************************
*
* parseNotification -
*
*/
static std::string parseNotification(ConnectionInfo* ciP, SubscribeContextRequest* scrP, const Value& notification)
{
  if (!notification.IsObject())
  {
    OrionError oe(SccBadRequest, "notitication is not an object");

    alarmMgr.badInput(clientIp, "notification is not an object");
    return oe.render(ciP, "");

  }

  // Callback
  if (notification.HasMember("http"))
  {
    const Value& http = notification["http"];
    if (!http.IsObject())
    {
      OrionError oe(SccBadRequest, "http notitication is not an object");

      alarmMgr.badInput(clientIp, "http notification is not an object");
      return oe.render(ciP, "");

    }

    if (!http.HasMember("url"))
    {
      alarmMgr.badInput(clientIp, "url http notification is missing");
      OrionError oe(SccBadRequest, "url http notification is missing");

      return oe.render(ciP, "");
    }
    const Value& url = http["url"];
    if (!url.IsString())
    {
      alarmMgr.badInput(clientIp, "url http notification is not a string");
      OrionError oe(SccBadRequest, "url http notification is not a string");

      return oe.render(ciP, "");
    }
    scrP->reference.string = url.GetString();

    std::string refError = scrP->reference.check(SubscribeContext, "" ,"", 0);
    if (refError != "OK")
    {
      alarmMgr.badInput(clientIp, refError);
      OrionError oe(SccBadRequest, refError + " parsing notification url");

      return oe.render(ciP, "");
    }
  }
  else  // missing callback field
  {
    alarmMgr.badInput(clientIp, "http notification is missing");
    OrionError oe(SccBadRequest, "http notification is missing");

    return oe.render(ciP, "");
  }

  // Attributes
  if (notification.HasMember("attrs"))
  {
    std::string r = parseAttributeList(ciP, &scrP->attributeList.attributeV, notification["attrs"]);

    if (r != "")
    {
      return r;
    }
  }

   return "";
}



/* ****************************************************************************
*
* parseNotifyConditionVector -
*
*/
static std::string parseNotifyConditionVector(ConnectionInfo* ciP, SubscribeContextRequest* scrP, const Value& condition)
{
  NotifyConditionVector* ncvP = &scrP->notifyConditionVector;

  if (!condition.IsObject())
  {
    alarmMgr.badInput(clientIp, "condition is not an object");
    OrionError oe(SccBadRequest, "condition is not an object");

    return oe.render(ciP, "");
  }
  // Attributes
  if (!condition.HasMember("attrs"))
  {
    alarmMgr.badInput(clientIp, "no condition attrs specified");
    OrionError oe(SccBadRequest, "no condition attrs specified");

    return oe.render(ciP, "");
  }

  NotifyCondition* nc = new NotifyCondition();
  nc->type = ON_CHANGE_CONDITION;
  ncvP->push_back(nc);

  std::string r = parseAttributeList(ciP, &nc->condValueList.vec, condition["attrs"]);

  if (r != "")
  {
    return r;
  }

  if (condition.HasMember("expression"))
  {
    const Value& expression = condition["expression"];

    if (!expression.IsObject())
    {
      alarmMgr.badInput(clientIp, "expression is not an object");
      OrionError oe(SccBadRequest, "expression is not an object");

      return oe.render(ciP, "");
    }

    scrP->expression.isSet = true;

    if (expression.HasMember("q"))
    {
      const Value& q = expression["q"];
      if (!q.IsString())
      {
        alarmMgr.badInput(clientIp, "q is not a string");
        OrionError oe(SccBadRequest, "q is not a string");

        return oe.render(ciP, "");
      }
      scrP->expression.q = q.GetString();

      std::string  errorString;
      Scope*       scopeP = new Scope(SCOPE_TYPE_SIMPLE_QUERY, expression["q"].GetString());

      scopeP->stringFilterP = new StringFilter();
      if (scopeP->stringFilterP->parse(scopeP->value.c_str(), &errorString) == false)
      {
        delete scopeP->stringFilterP;
        delete scopeP;

        alarmMgr.badInput(clientIp, errorString);
        OrionError oe(SccBadRequest, errorString);

        return oe.render(ciP, "");
      }

      scrP->restriction.scopeVector.push_back(scopeP);
    }
    if (expression.HasMember("geometry"))
    {
      const Value& geometry = expression["geometry"];
      if (!geometry.IsString())
      {
        alarmMgr.badInput(clientIp, "geometry is not a string");
        OrionError oe(SccBadRequest, "geometry is not a string");

        return oe.render(ciP, "");
      }
      scrP->expression.geometry = geometry.GetString();
    }
    if (expression.HasMember("coords"))
    {
      const Value& coords = expression["coords"];
      if (!coords.IsString())
      {
        alarmMgr.badInput(clientIp, "coords is not a string");
        OrionError oe(SccBadRequest, "coords is not a string");

        return oe.render(ciP, "");
      }
      scrP->expression.coords = coords.GetString();
    }
    if (expression.HasMember("georel"))
    {
      const Value& georel = expression["georel"];
      if (!georel.IsString())
      {
        alarmMgr.badInput(clientIp, "georel is not a string");
        OrionError oe(SccBadRequest, "georel is not a string");

        return oe.render(ciP, "");
      }
      scrP->expression.georel = georel.GetString();
    }
  }

  return "";
}



/* ****************************************************************************
*
* parseAttributeList -
*
*/
static std::string parseAttributeList(ConnectionInfo* ciP, std::vector<std::string>* vec, const Value& attributes)
{
  if (!attributes.IsArray())
  {
    alarmMgr.badInput(clientIp, "attrs is not an array");
    OrionError oe(SccBadRequest, "attrs is not an array");

    return oe.render(ciP, "");
  }

  for (Value::ConstValueIterator iter = attributes.Begin(); iter != attributes.End(); ++iter)
  {
    if (!iter->IsString())
    {
      alarmMgr.badInput(clientIp, "attrs element is not an string");
      OrionError oe(SccBadRequest, "attrs element is not an string");

      return oe.render(ciP, "");
    }

    vec->push_back(iter->GetString());
  }

  return "";
}
