// checkbox.cpp - GUICheckbox object

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include <string>

extern "C" {
#include "../common.h"
#include "../minui/minui.h"
#include "../recovery_ui.h"
}

#include "rapidxml.hpp"
#include "objects.hpp"

Conditional::Conditional(xml_node<>* node)
{
    // Break out early, it's too hard to check if valid every step
    if (!node)          return;

    // First, get the action
    xml_node<>* condition = node->first_node("conditions");
    if (condition)  condition = condition->first_node("condition");
    else            condition = node->first_node("condition");

    if (!condition)     return;

    while (condition)
    {
        Condition cond;

        cond.mCompareOp = "=";

        xml_attribute<>* attr;
        attr = condition->first_attribute("var1");
        if (attr)   cond.mVar1 = attr->value();

        attr = condition->first_attribute("op");
        if (attr)   cond.mCompareOp = attr->value();

        attr = condition->first_attribute("var2");
        if (attr)   cond.mVar2 = attr->value();
    
        mConditions.push_back(cond);

        condition = condition->next_sibling("condition");
    }
}

bool Conditional::IsConditionVariable(std::string var)
{
    std::vector<Condition>::iterator iter;
    for (iter = mConditions.begin(); iter != mConditions.end(); iter++)
    {
        if (iter->mVar1 == var)     return true;
    }
    return false;
}

bool Conditional::isConditionTrue()
{
    std::vector<Condition>::iterator iter;
    for (iter = mConditions.begin(); iter != mConditions.end(); iter++)
    {
        if (!isConditionTrue(&(*iter)))     return false;
    }
    return true;
}

bool Conditional::isConditionTrue(Condition* condition)
{
    // This is used to hold the proper value of "true" based on the '!' NOT flag
    bool bTrue = true;

    if (condition->mVar1.empty())      return bTrue;

    if (!condition->mCompareOp.empty() && condition->mCompareOp[0] == '!')
        bTrue = false;

    if (condition->mVar2.empty() && condition->mCompareOp != "modified")
    {
        if (!DataManager::GetStrValue(condition->mVar1).empty())
            return bTrue;

        return !bTrue;
    }

    string var1, var2;
    if (DataManager::GetValue(condition->mVar1, var1))
        var1 = condition->mVar1;
    if (DataManager::GetValue(condition->mVar2, var2))
        var2 = condition->mVar2;

    // This is a special case, we stat the file and that determines our result
    if (var1 == "fileexists")
    {
        struct stat st;
        if (stat(var2.c_str(), &st) == 0)
            var2 = var1;
        else
            var2 = "FAILED";
    }
    if (var1 == "mounted")
    {
        if (isMounted(condition->mVar2))
            var2 = var1;
        else
            var2 = "FAILED";
    }

    if (condition->mCompareOp.find('=') != string::npos && var1 == var2)
        return bTrue;

    if (condition->mCompareOp.find('>') != string::npos && (atof(var1.c_str()) > atof(var2.c_str())))
        return bTrue;

    if (condition->mCompareOp.find('<') != string::npos && (atof(var1.c_str()) < atof(var2.c_str())))
        return bTrue;

    if (condition->mCompareOp == "modified")
    {
        // This is a hack to allow areas to reset the default value
        if (var1.empty())
        {
            condition->mLastVal = var1;
            return !bTrue;
        }

        if (var1 != condition->mLastVal)
            return bTrue;
    }

    return !bTrue;
}

bool Conditional::isConditionValid()
{
    return !mConditions.empty();
}

void Conditional::NotifyPageSet()
{
    std::vector<Condition>::iterator iter;
    for (iter = mConditions.begin(); iter != mConditions.end(); iter++)
    {
        if (iter->mCompareOp == "modified")
        {
            string val;
    
            // If this fails, val will not be set, which is perfect
            if (DataManager::GetValue(iter->mVar1, val))
            {
                DataManager::SetValue(iter->mVar1, "");
                DataManager::GetValue(iter->mVar1, val);
            }
            iter->mLastVal = val;
        }
    }
}

bool Conditional::isMounted(string vol)
{
	FILE *fp;
    char tmpOutput[255];

	fp = fopen("/proc/mounts", "rt");
	while (fgets(tmpOutput,255,fp) != NULL)
	{
        char* mnt = tmpOutput;
        while (*mnt > 32)               mnt++;
        while (*mnt > 0 && *mnt <= 32)  mnt++;
        char* pos = mnt;
	    while (*pos > 32)   pos++;
        *pos = 0;
	    if (vol == mnt)
        {
            fclose(fp);
            return true;
        }
    }
    fclose(fp);
    return false;
}

