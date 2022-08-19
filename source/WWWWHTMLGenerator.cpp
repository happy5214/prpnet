#include <cstdarg>

#include "WWWWHTMLGenerator.h"
#include "WWWWServerHelper.h"
#include "ServerHelper.h"
#include "SQLStatement.h"

WWWWHTMLGenerator::WWWWHTMLGenerator(globals_t *theGlobals) : HTMLGenerator(theGlobals)
{
   switch (ii_ServerType) {
      case ST_WIEFERICH:
         strcpy(ic_SearchType, "Wieferich");
         break;
      case ST_WILSON:
         strcpy(ic_SearchType, "Wilson");
         break;
      case ST_WALLSUNSUN:
         strcpy(ic_SearchType, "Wall-Sun-Sun");
         break;
      case ST_WOLSTENHOLME:
         strcpy(ic_SearchType, "Wolstenholme");
         break;
   }
}

bool     WWWWHTMLGenerator::SendSpecificPage(string thePage)
{
   if (thePage == "user_finds.html")
   {
      HeaderPlusLinks("Finds by User");
      FindsByUser();
   }
   else if (thePage == "team_finds.html" && HasTeams())
   {
      HeaderPlusLinks("Finds by Team");
      FindsByTeam();
   }
   else if (thePage == "pending_work.html")
   {
      HeaderPlusLinks("Pending Work");
      PendingTests();
   }
   else if (thePage == "all.html")
   {
      HeaderPlusLinks("Status");

      PendingTests();
      ServerStats();
      UserStats();
      if (HasTeams())
      {
         TeamStats();
         FindsByUser();
         FindsByTeam();
         TeamUserStats();
      }
   }
   else
      return false;

   return true;
}

void     WWWWHTMLGenerator::PendingTests(void)
{
   SQLStatement *sqlStatement;
   int64_t     testID, expireSeconds;
   char        userID[ID_LENGTH+1], machineID[ID_LENGTH+1];
   char        instanceID[ID_LENGTH+1], teamID[ID_LENGTH+1];
   string      lowerLimitStr, upperLimitStr;
   int64_t     lowerLimit, upperLimit;
   int32_t     seconds, hours, minutes;
   int32_t     expireHours, expireMinutes;
   time_t      currentTime;

   const char *theSelect = "select WWWWRangeTest.LowerLimit, WWWWRangeTest.UpperLimit, " \
                           "       WWWWRangeTest.TestID, WWWWRangeTest.UserID, " \
                           "       WWWWRangeTest.MachineID, WWWWRangeTest.InstanceID, " \
                           "       $null_func$(WWWWRangeTest.TeamID, ' ') " \
                           "  from WWWWRange, WWWWRangeTest" \
                           " where WWWWRange.LowerLimit = WWWWRangeTest.LowerLimit " \
                           "   and WWWWRange.UpperLimit = WWWWRangeTest.UpperLimit " \
                           "   and WWWWRange.HasPendingTest = 1 " \
                           "   and WWWWRangeTest.SearchingProgram is null " \
                           "order by WWWWRangeTest.TestID, WWWWRangeTest.LowerLimit ";

   sqlStatement = new SQLStatement(ip_Log, ip_DBInterface, theSelect);

   sqlStatement->BindSelectedColumn(&lowerLimit);
   sqlStatement->BindSelectedColumn(&upperLimit);
   sqlStatement->BindSelectedColumn(&testID);
   sqlStatement->BindSelectedColumn(userID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(machineID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(instanceID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(teamID, ID_LENGTH);

   currentTime = time(NULL);

   ip_Socket->Send("<article>");

   if (!CheckIfRecordsWereFound(sqlStatement, "No Pending WU\'s"))
      return;

   ip_Socket->Send("<table id=\"pending-tests-table\"><thead>");
   ip_Socket->Send("<tr><th colspan=\"8\">Pending WU\'s</th></tr><tr>");

   TH_CLMN_HDR("Range");
   TH_CLMN_HDR("User");
   TH_CLMN_HDR("Machine");
   TH_CLMN_HDR("Instance");
   TH_CLMN_HDR("Team");
   TH_CLMN_HDR("Date Assigned");
   TH_CLMN_HDR("Age (hh:mm)");
   TH_CLMN_HDR("Expires (hh:mm)");

   ip_Socket->Send("</tr></thead><tbody>");

   do
   {
      expireSeconds = testID + ip_Delay[0].expireDelay - (int64_t) currentTime;

      if (expireSeconds < 0) expireSeconds = 0;
      expireHours = (int32_t) (expireSeconds / 3600);
      expireMinutes = (int32_t) (expireSeconds - expireHours * 3600) / 60;

      seconds = (int32_t) (currentTime - testID);
      hours = seconds / 3600;
      minutes = (seconds - hours * 3600) / 60;

      ConvertToScientificNotation(lowerLimit, lowerLimitStr);
      ConvertToScientificNotation(upperLimit, upperLimitStr);

      ip_Socket->Send("<tr><th scope=\"row\">%s - %s</th>",
                        lowerLimitStr.c_str(), upperLimitStr.c_str());

      TD_CHAR(userID);
      TD_CHAR(machineID);
      TD_CHAR(instanceID);
      TD_CHAR(teamID);
      TD_CHAR(TimeToString(testID).c_str());
      TD_TIME(hours, minutes);
      TD_TIME(expireHours, expireMinutes);

      ip_Socket->Send("</tr>");
   } while (sqlStatement->FetchRow(false));

   ip_Socket->Send("</tbody></table></article>");

   delete sqlStatement;
}

void     WWWWHTMLGenerator::FindsByUser(void)
{
   SQLStatement *sqlStatement;
   int64_t    dateReported, prime;
   int32_t    remainder, quotient;
   int32_t    foundCount, nearCount, showOnWebPage, hiddenCount;
   char       userID[ID_LENGTH+1], prevUserID[ID_LENGTH+1];
   char       machineID[ID_LENGTH+1], instanceID[ID_LENGTH+1], teamID[ID_LENGTH+1];

   const char *theSelect = "select UserID, TeamID, MachineID, InstanceID, Prime, Remainder, " \
                           "       Quotient, DateReported, ShowOnWebPage " \
                           "  from UserWWWWs " \
                           "order by UserID, Prime";

   sqlStatement = new SQLStatement(ip_Log, ip_DBInterface, theSelect);

   sqlStatement->BindSelectedColumn(userID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(teamID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(machineID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(instanceID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(&prime);
   sqlStatement->BindSelectedColumn(&remainder);
   sqlStatement->BindSelectedColumn(&quotient);
   sqlStatement->BindSelectedColumn(&dateReported);
   sqlStatement->BindSelectedColumn(&showOnWebPage);

   ip_Socket->Send("<article><h2>%s and Near %s Found by User</h2>", ic_SearchType, ic_SearchType);

   if (!CheckIfRecordsWereFound(sqlStatement, "No %s or Near %s found", ic_SearchType, ic_SearchType))
      return;

   ip_Socket->StartBuffering();

   *prevUserID = 0;
   hiddenCount = foundCount = nearCount = 0;
   ip_Socket->Send("<table id=\"finds-by-user-table\">");
   do
   {
      if (strcmp(prevUserID, userID))
      {
         if (*prevUserID)
         {
            if (hiddenCount)
               ip_Socket->Send("<tr><td style=\"text-align: center;\" colspan=\"7\">User %s has found %d %s%s and %d Near %s%s, %d %s hidden</td></tr>",
                                 prevUserID, foundCount, ic_SearchType, PLURAL_ENDING(foundCount),
                                 nearCount, ic_SearchType, PLURAL_ENDING(nearCount),
                                 hiddenCount, PLURAL_COPULA(hiddenCount));
            else
               ip_Socket->Send("<tr><td style=\"text-align: center;\" colspan=\"7\">User %s has found %d %s%s and %d Near %s%s</td></tr>",
                                 prevUserID, foundCount, ic_SearchType, PLURAL_ENDING(foundCount),
                                 nearCount, ic_SearchType, PLURAL_ENDING(nearCount));
            ip_Socket->Send("<tr><td colspan=\"6\">&nbsp;</td></tr></tbody>");
         }

         ip_Socket->Send("<tbody><tr class=\"headercolor\"><th style=\"text-align: center;\" colspan=\"7\">%s and Near %s for User %s</th></tr>",
                           ic_SearchType, ic_SearchType, userID);
         ip_Socket->Send("<tr class=\"headercolor\">");

         TH_CLMN_HDR("Value");
         TH_CLMN_HDR("Team");
         TH_CLMN_HDR("Machine");
         TH_CLMN_HDR("Instance");
         TH_CLMN_HDR("Date Reported");

         ip_Socket->Send("</tr>");

         strcpy(prevUserID, userID);
         hiddenCount = foundCount = nearCount = 0;
      }

      if (!remainder && !quotient)
         foundCount++;
      else
         nearCount++;

      if (showOnWebPage)
      {
         ip_Socket->Send("<tr>");
         if (!remainder && !quotient)
         {
            ip_Socket->Send("<th scope=\"row\">%" PRId64"</th>", prime);

            TD_CHAR(teamID);
            TD_CHAR(machineID);
            TD_CHAR(instanceID);
            TD_CHAR(TimeToString(dateReported).c_str());
         }
         else
         {
            if (ii_ServerType == ST_WALLSUNSUN)
            {
               ip_Socket->Send("<th scope=\"row\">%" PRId64" (0 %+d p)</th>", prime, quotient);

               TD_CHAR(teamID);
               TD_CHAR(machineID);
               TD_CHAR(instanceID);
               TD_CHAR(TimeToString(dateReported).c_str());
            }
            else
            {
               ip_Socket->Send("<th scope=\"row\">%" PRId64" (%+d %+d p)</th>",
                                 prime, remainder, quotient);

               TD_CHAR(teamID);
               TD_CHAR(machineID);
               TD_CHAR(instanceID);
               TD_CHAR(TimeToString(dateReported).c_str());
            }
         }
         ip_Socket->Send("</tr>");
      }
      else
         hiddenCount++;
   } while (sqlStatement->FetchRow(false));

   if (hiddenCount)
      ip_Socket->Send("<tr><td style=\"text-align: center;\" colspan=\"6\">User %s has found %d %s%s and %d Near %s%s, %d %s hidden</td></tr>",
                        prevUserID, foundCount, ic_SearchType, PLURAL_ENDING(foundCount),
                        nearCount, ic_SearchType, PLURAL_ENDING(nearCount),
                        hiddenCount, PLURAL_COPULA(hiddenCount));
   else
      ip_Socket->Send("<tr><td style=\"text-align: center;\" colspan=\"6\">User %s has found %d %s%s and %d Near %s%s</td></tr>",
                        prevUserID, foundCount, ic_SearchType, PLURAL_ENDING(foundCount),
                        nearCount, ic_SearchType, PLURAL_ENDING(nearCount));
   ip_Socket->Send("</tbody></table></article>");

   ip_Socket->SendBuffer();

   delete sqlStatement;
}

void     WWWWHTMLGenerator::FindsByTeam(void)
{
   SQLStatement *sqlStatement;
   int64_t    dateReported, prime, remainder, quotient;
   int32_t    foundCount, nearCount, showOnWebPage, hiddenCount;
   char       userID[ID_LENGTH+1], prevTeamID[ID_LENGTH+1];
   char       machineID[ID_LENGTH+1], instanceID[ID_LENGTH+1], teamID[ID_LENGTH+1];

   const char *theSelect = "select UserID, TeamID, MachineID, InstanceID, Prime, " \
                           "       Remainder, Quotient, DateReported, ShowOnWebPage " \
                           "  from UserWWWWs " \
                           " where $null_func$(TeamID, ' ') <> ' ' " \
                           "order by TeamID, Prime";

   sqlStatement = new SQLStatement(ip_Log, ip_DBInterface, theSelect);

   sqlStatement->BindSelectedColumn(userID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(teamID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(machineID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(instanceID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(&prime);
   sqlStatement->BindSelectedColumn(&remainder);
   sqlStatement->BindSelectedColumn(&quotient);
   sqlStatement->BindSelectedColumn(&dateReported);
   sqlStatement->BindSelectedColumn(&showOnWebPage);

   ip_Socket->Send("<article><h2>%s and Near %s Found by Team</h2>", ic_SearchType, ic_SearchType);

   if (!CheckIfRecordsWereFound(sqlStatement, "No %s or Near %s found", ic_SearchType, ic_SearchType))
      return;

   ip_Socket->StartBuffering();

   *prevTeamID = 0;
   hiddenCount = foundCount = nearCount = 0;
   ip_Socket->Send("<table id=\"finds-by-team-table\">");
   do
   {
      if (strcmp(prevTeamID, teamID))
      {
         if (*prevTeamID)
         {
            if (hiddenCount)
               ip_Socket->Send("<tr><td style=\"text-align: center;\" colspan=\"7\">Team %s has found %d %s%s and %d Near %s%s, %d %s hidden</td></tr>",
                                 prevTeamID, foundCount, ic_SearchType, PLURAL_ENDING(foundCount),
                                 nearCount, ic_SearchType, PLURAL_ENDING(nearCount),
                                 hiddenCount, PLURAL_COPULA(hiddenCount));
            else
               ip_Socket->Send("<tr><td style=\"text-align: center;\" colspan=\"7\">Team %s has found %d %s%s and %d Near %s%s</td></tr>",
                                 prevTeamID, foundCount, ic_SearchType, PLURAL_ENDING(foundCount),
                                 nearCount, ic_SearchType, PLURAL_ENDING(nearCount));
            ip_Socket->Send("<tr><td colspan=\"6\">&nbsp;</td></tr></tbody>");
         }

         ip_Socket->Send("<tbody><tr class=\"headercolor\"><th style=\"text-align: center;\" colspan=\"7\">%s and Near %s for Team %s</th></tr>",
                           ic_SearchType, ic_SearchType, teamID);
         ip_Socket->Send("<tr class=\"headercolor\"><th>Value</th><th scope=\"col\">User</th>");
         ip_Socket->Send("<th scope=\"col\">Machine</th><th scope=\"col\">Instance</th><th scope=\"col\">Date Reported</th></tr>");

         strcpy(prevTeamID, teamID);
         hiddenCount = foundCount = nearCount = 0;
      }

      if (!remainder && !quotient)
         foundCount++;
      else
         nearCount++;

      if (showOnWebPage)
      {
         if (!remainder && !quotient)
            ip_Socket->Send("<tr><td style=\"text-align: center;\">%" PRId64"</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                              prime, userID, machineID, instanceID, TimeToString(dateReported).c_str());
         else
         {
            if (ii_ServerType == ST_WALLSUNSUN)
               ip_Socket->Send("<tr><td style=\"text-align: center;\">%" PRId64" (0 %+d p)</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                                 prime, quotient, userID, machineID, instanceID, TimeToString(dateReported).c_str());
            else
               ip_Socket->Send("<tr><td style=\"text-align: center;\">%" PRId64" (%+d %+d p)</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
                                 prime, remainder, quotient, userID, machineID, instanceID, TimeToString(dateReported).c_str());
         }
      }
      else
         hiddenCount++;
   } while (sqlStatement->FetchRow(false));

   if (hiddenCount)
      ip_Socket->Send("<tr><td style=\"text-align: center;\" colspan=\"6\">Team %s has found %d %s%s and %d Near %s%s, %d %s hidden</td></tr>",
                        prevTeamID, foundCount, ic_SearchType, PLURAL_ENDING(foundCount),
                        nearCount, ic_SearchType, PLURAL_ENDING(nearCount),
                        hiddenCount, PLURAL_COPULA(hiddenCount));
   else
      ip_Socket->Send("<tr><td style=\"text-align: center;\" colspan=\"6\">Team %s has found %d %s%s and %d Near %s%s</td></tr>",
                        prevTeamID, foundCount, ic_SearchType, PLURAL_ENDING(foundCount),
                        nearCount, ic_SearchType, PLURAL_ENDING(nearCount));
   ip_Socket->Send("</tbody></table></article>");

   ip_Socket->SendBuffer();

   delete sqlStatement;
}

void  WWWWHTMLGenerator::ServerStats(void)
{
   SQLStatement *sqlStatement;
   int64_t     minInGroup, maxInGroup, completedThru, leadingEdge;
   int32_t     countInGroup, countInProgress, countedTested, countDoubleChecked;
   int32_t     countUntested, findCount, nearCount;
   string      minInGroupStr, maxInGroupStr;
   string      completedThruStr, leadingEdgeStr;

   const char *theSelect = "select CountInGroup, MinInGroup, MaxInGroup, " \
                           "       CountTested, CountDoubleChecked, CountUntested, " \
                           "       CountInProgress, CompletedThru, LeadingEdge, " \
                           "       WWWWPrimes, NearWWWWPrimes " \
                           "  from WWWWGroupStats ";

   sqlStatement = new SQLStatement(ip_Log, ip_DBInterface, theSelect);

   sqlStatement->BindSelectedColumn(&countInGroup);
   sqlStatement->BindSelectedColumn(&minInGroup);
   sqlStatement->BindSelectedColumn(&maxInGroup);
   sqlStatement->BindSelectedColumn(&countedTested);
   sqlStatement->BindSelectedColumn(&countDoubleChecked);
   sqlStatement->BindSelectedColumn(&countUntested);
   sqlStatement->BindSelectedColumn(&countInProgress);
   sqlStatement->BindSelectedColumn(&completedThru);
   sqlStatement->BindSelectedColumn(&leadingEdge);
   sqlStatement->BindSelectedColumn(&findCount);
   sqlStatement->BindSelectedColumn(&nearCount);

   ip_Socket->Send("<article><h2>Server Stats</h2>");

   if (!CheckIfRecordsWereFound(sqlStatement, "No group stats found"))
      return;

   ip_Socket->StartBuffering();

   ip_Socket->Send("<table id=\"server-stats-table\"><thead>");
   ip_Socket->Send("<tr class=\"headercolor\"><th style=\"text-align: center;\" colspan=\"%d\">Server Stats</th></tr>",
                   (ib_NeedsDoubleCheck ? 11 : 10));
   ip_Socket->Send("<tr><th scope=\"col\">Total Ranges</th><th scope=\"col\">Min Range Start</th>");
   ip_Socket->Send("<th scope=\"col\">Max Range Start</th><th scope=\"col\">Count Tested</th>");
   if (ib_NeedsDoubleCheck) ip_Socket->Send("<th scope=\"col\">Count DC\'d</th>");
   ip_Socket->Send("<th scope=\"col\">Count Untested</th><th scope=\"col\">In Progress</th>");
   ip_Socket->Send("<th scope=\"col\">Completed Thru</th><th scope=\"col\">Leading Edge</th>");
   ip_Socket->Send("<th scope=\"col\">%ss</th><th scope=\"col\">Near %ss</th>", ic_SearchType, ic_SearchType);
   ip_Socket->Send("</tr></thead><tbody>");

   do
   {
      // If the socket was closed, then stop sending data
      if (!ip_Socket->Send("<tr class=\"%s\">", (countUntested ? "untested" : "tested")))
         break;

      ConvertToScientificNotation(minInGroup, minInGroupStr);
      ConvertToScientificNotation(maxInGroup, maxInGroupStr);
      ConvertToScientificNotation(completedThru, completedThruStr);
      ConvertToScientificNotation(leadingEdge, leadingEdgeStr);

      TD_32BIT(countInGroup);
      ip_Socket->Send("<td style=\"text-align: right;\">%s</td>", minInGroupStr.c_str());
      ip_Socket->Send("<td style=\"text-align: right;\">%s</td>", maxInGroupStr.c_str());
      TD_32BIT(countedTested);
      TD_IF_DC(countDoubleChecked);
      TD_32BIT(countUntested);
      TD_32BIT(countInProgress);
      ip_Socket->Send("<td style=\"text-align: right;\">%s</td>", completedThruStr.c_str());
      ip_Socket->Send("<td style=\"text-align: right;\">%s</td>", leadingEdgeStr.c_str());
      TD_32BIT(findCount);
      TD_32BIT(nearCount);

      ip_Socket->Send("</tr>");
   } while (sqlStatement->FetchRow(false));

   ip_Socket->Send("</tbody></table></article>");

   ip_Socket->SendBuffer();

   delete sqlStatement;
}

void  WWWWHTMLGenerator::UserStats(void)
{
   SQLStatement *sqlStatement;
   char        userID[ID_LENGTH+1];
   int32_t     testsPerformed, findCount, nearCount;
   double      totalScore;

   const char *theSelect = "select UserID, TestsPerformed, WWWWPrimes, " \
                           "       NearWWWWPrimes, TotalScore " \
                           "  from UserStats " \
                           "order by TotalScore desc";

   sqlStatement = new SQLStatement(ip_Log, ip_DBInterface, theSelect);

   sqlStatement->BindSelectedColumn(userID, sizeof(userID));
   sqlStatement->BindSelectedColumn(&testsPerformed);
   sqlStatement->BindSelectedColumn(&findCount);
   sqlStatement->BindSelectedColumn(&nearCount);
   sqlStatement->BindSelectedColumn(&totalScore);

   ip_Socket->Send("<article><h2>User Stats</h2>");

   if (!CheckIfRecordsWereFound(sqlStatement, "No user stats found"))
      return;

   ip_Socket->StartBuffering();
   ip_Socket->Send("<table style=\"margin-bottom: 1em;\">");
   ip_Socket->Send("<tr class=\"headercolor\"><th style=\"text-align: center;\" colspan=\"6\">User Stats</th></tr>");
   ip_Socket->Send("</table>");
   ip_Socket->Send("<table id=\"user-stats-table\" class=\"sortable\"><thead>");
   ip_Socket->Send("<tr><th scope=\"col\">User</th>");
   ip_Socket->Send("<th scope=\"col\" class=\"sorttable_numeric\">Total Score</th><th scope=\"col\" class=\"sorttable_numeric\">RangesTested</th>");
   ip_Socket->Send("<th scope=\"col\" class=\"sorttable_numeric\">%ss</th><th scope=\"col\" class=\"sorttable_numeric\">Near %ss</th></tr>", ic_SearchType, ic_SearchType);
   ip_Socket->Send("</thead><tbody>");

   do
   {
      ip_Socket->Send("<tr><td>%s</td><td style=\"text-align: right;\">%.0lf</td><td style=\"text-align: right;\">%d</td>",
                        userID, totalScore, testsPerformed);
      ip_Socket->Send("<td style=\"text-align: right;\">%d</td><td style=\"text-align: right;\">%d</td></tr>",
                        findCount, nearCount);
   } while (sqlStatement->FetchRow(false));

   ip_Socket->Send("</tbody></table></article>");
   ip_Socket->SendBuffer();

   delete sqlStatement;
}

void  WWWWHTMLGenerator::UserTeamStats(void)
{
   SQLStatement *sqlStatement;
   char        userID[ID_LENGTH+1], teamID[ID_LENGTH+1];
   int32_t     testsPerformed, findCount, nearCount;
   double      totalScore;

   const char *theSelect = "select UserID, TeamID, TestsPerformed, WWWWPrimes, " \
                           "       NearWWWWPrimes, TotalScore " \
                           "  from UserTeamStats " \
                           "order by UserID, TotalScore, TeamID";

   sqlStatement = new SQLStatement(ip_Log, ip_DBInterface, theSelect);

   sqlStatement->BindSelectedColumn(userID, sizeof(userID));
   sqlStatement->BindSelectedColumn(teamID, sizeof(teamID));
   sqlStatement->BindSelectedColumn(&testsPerformed);
   sqlStatement->BindSelectedColumn(&findCount);
   sqlStatement->BindSelectedColumn(&nearCount);
   sqlStatement->BindSelectedColumn(&totalScore);

   ip_Socket->Send("<article><h2>User Team Stats</h2>");

   if (!CheckIfRecordsWereFound(sqlStatement, "No user team stats found"))
      return;

   ip_Socket->StartBuffering();
   ip_Socket->Send("<table id=\"user-team-stats-table\"><thead>");
   ip_Socket->Send("<tr class=\"headercolor\"><th style=\"text-align: center;\" colspan=\"6\">User Team Stats</th></tr>");
   ip_Socket->Send("<tr><th scope=\"col\">User</th>");
   ip_Socket->Send("<th scope=\"col\">Team</th><th scope=\"col\">Total Score</th><th scope=\"col\">Ranges Tested</th>");
   ip_Socket->Send("<th scope=\"col\">%ss</th><th scope=\"col\">Near %ss</th></tr>", ic_SearchType, ic_SearchType);
   ip_Socket->Send("</thead><tbody>");

   do
   {
      ip_Socket->Send("<tr><td>%s</td><td>%s</td><td style=\"text-align: right;\">%.0lf</td><td style=\"text-align: right;\">%d</td>",
                        userID, teamID, totalScore, testsPerformed);
      ip_Socket->Send("<td style=\"text-align: right;\">%d</td><td style=\"text-align: right;\">%d</td></tr>",
                        findCount, nearCount);
   } while (sqlStatement->FetchRow(false));

   ip_Socket->Send("</tbody></table></article>");
   ip_Socket->SendBuffer();

   delete sqlStatement;
}

void  WWWWHTMLGenerator::TeamUserStats(void)
{
   SQLStatement *sqlStatement;
   char        userID[ID_LENGTH+1], teamID[ID_LENGTH+1];
   int32_t     testsPerformed, findCount, nearCount;
   double      totalScore;

   const char *theSelect = "select UserID, TeamID, TestsPerformed, WWWWPrimes, " \
                           "       NearWWWWPrimes, TotalScore " \
                           "  from UserTeamStats " \
                           "order by TeamID, TotalScore, UserID";

   sqlStatement = new SQLStatement(ip_Log, ip_DBInterface, theSelect);

   sqlStatement->BindSelectedColumn(userID, sizeof(userID));
   sqlStatement->BindSelectedColumn(teamID, sizeof(teamID));
   sqlStatement->BindSelectedColumn(&testsPerformed);
   sqlStatement->BindSelectedColumn(&findCount);
   sqlStatement->BindSelectedColumn(&nearCount);
   sqlStatement->BindSelectedColumn(&totalScore);

   ip_Socket->Send("<article><h2>User Stats by Team</h2>");

   if (!CheckIfRecordsWereFound(sqlStatement, "No teams found"))
      return;

   ip_Socket->StartBuffering();
   ip_Socket->Send("<table id=\"team-user-stats-table\"><thead>");
   ip_Socket->Send("<tr class=\"headercolor\"><th style=\"text-align: center;\" colspan=\"6\">User Stats by Team</th></tr>");
   ip_Socket->Send("<tr><th scope=\"col\">Team</th><th scope=\"col\">User</th>");
   ip_Socket->Send("<th scope=\"col\">Total Score</th><th scope=\"col\">Ranges Tested</th>");
   ip_Socket->Send("<th scope=\"col\">%ss</th><th scope=\"col\">Near %ss</th></tr>", ic_SearchType, ic_SearchType);
   ip_Socket->Send("</thead><tbody>");

   do
   {
      ip_Socket->Send("<tr><td>%s</td><td>%s</td><td style=\"text-align: right;\">%.0lf</td><td style=\"text-align: right;\">%d</td>",
                        teamID, userID, totalScore, testsPerformed);
      ip_Socket->Send("<td style=\"text-align: right;\">%d</td><td style=\"text-align: right;\">%d</td></tr>",
                        findCount, nearCount);
   } while (sqlStatement->FetchRow(false));

   ip_Socket->Send("</tbody></table></article>");
   ip_Socket->SendBuffer();

   delete sqlStatement;
}

void  WWWWHTMLGenerator::TeamStats(void)
{
   SQLStatement *sqlStatement;
   char        teamID[ID_LENGTH+1];
   int32_t     testsPerformed, findCount, nearCount;
   double      totalScore;

   const char *theSelect = "select TeamID, TestsPerformed, WWWWPrimes, " \
                           "       NearWWWWPrimes, TotalScore " \
                           "  from TeamStats " \
                           "order by TotalScore desc";

   sqlStatement = new SQLStatement(ip_Log, ip_DBInterface, theSelect);

   sqlStatement->BindSelectedColumn(teamID, sizeof(teamID));
   sqlStatement->BindSelectedColumn(&testsPerformed);
   sqlStatement->BindSelectedColumn(&findCount);
   sqlStatement->BindSelectedColumn(&nearCount);
   sqlStatement->BindSelectedColumn(&totalScore);

   ip_Socket->Send("<article><h2>Team Stats</h2>");

   if (!CheckIfRecordsWereFound(sqlStatement, "No team stats found"))
      return;

   ip_Socket->StartBuffering();
   ip_Socket->Send("<table style=\"margin-bottom: 1em;\">");
   ip_Socket->Send("<tr class=\"headercolor\"><th style=\"text-align: center;\" colspan=\"6\">Team Stats</th></tr>");
   ip_Socket->Send("</table>");
   ip_Socket->Send("<table id=\"team-stats-table\" class=\"sortable\"><thead>");
   ip_Socket->Send("<tr><th scope=\"col\">Team</th>");
   ip_Socket->Send("<th scope=\"col\" class=\"sorttable_numeric\">Total Score</th><th scope=\"col\" class=\"sorttable_numeric\">Tests Performed</th>");
   ip_Socket->Send("<th scope=\"col\" class=\"sorttable_numeric\">%ss</th><th scope=\"col\" class=\"sorttable_numeric\">Near %ss</th></tr>", ic_SearchType, ic_SearchType);
   ip_Socket->Send("</thead><tbody>");

   do
   {
      ip_Socket->Send("<tr><td>%s</td><td style=\"text-align: right;\">%.0lf</td><td style=\"text-align: right;\">%d</td>",
                        teamID, totalScore, testsPerformed);
      ip_Socket->Send("<td style=\"text-align: right;\">%d</td><td style=\"text-align: right;\">%d</td></tr>",
                        findCount, nearCount);
   } while (sqlStatement->FetchRow(false));

   ip_Socket->Send("</tbody></table></article>");
   ip_Socket->SendBuffer();

   delete sqlStatement;
}

void  WWWWHTMLGenerator::ConvertToScientificNotation(int64_t valueInt, string &valueStr)
{
   int32_t  eValue = 0, maxE;
   char     tempValue[50];

   if (ii_ServerType == ST_WIEFERICH)    maxE = 11;
   if (ii_ServerType == ST_WILSON)       maxE = 6;
   if (ii_ServerType == ST_WALLSUNSUN)   maxE = 10;
   if (ii_ServerType == ST_WOLSTENHOLME) maxE = 7;

   if (valueInt == 0)
   {
      valueStr = "0";
      return;
   }

   eValue = 0;
   while (valueInt % 10 == 0 && eValue < maxE)
   {
      eValue++;
      valueInt /= 10;
   }

   sprintf(tempValue, "%" PRId64"e%d", valueInt, eValue);
   valueStr = tempValue;
}

void     WWWWHTMLGenerator::SendLinks(string pageTitle)
{
   ip_Socket->Send("<nav style=\"margin-bottom: 2em;\"><table style=\"font-weight: bold; font-size: large;\"><tr>");

   ip_Socket->Send("<td><a href=\"server_stats.html\">Server Statistics</a></td>");
   ip_Socket->Send("<td><a href=\"pending_work.html\">Pending Work</a></td>");
   ip_Socket->Send("<td><a href=\"user_stats.html\">User Statistics</a></td>");
   ip_Socket->Send("<td><a href=\"user_finds.html\">User Finds</a></td>");
   if (HasTeams())
   {
      ip_Socket->Send("<tr><td><a href=\"team_stats.html\">Team Statistics</a></td>");
      ip_Socket->Send("<td><a href=\"team_finds.html\">Team Finds</a></td>");
      ip_Socket->Send("<td><a href=\"userteam_stats.html\">User/Team Statistics</a></td>");
      ip_Socket->Send("<td><a href=\"teamuser_stats.html\">Team/User Statistics</a></td>");
   }
   ip_Socket->Send("</tr></table></nav>");
}

ServerHelper *WWWWHTMLGenerator::GetServerHelper(void)
{
   ServerHelper *serverHelper = new WWWWServerHelper(ip_DBInterface, ip_Log);
   return serverHelper;
}

bool WWWWHTMLGenerator::CheckIfRecordsWereFound(SQLStatement *sqlStatement, string noRecordsFoundMessage, ...)
{
   char     buf[100];
   va_list  args;

   va_start(args, noRecordsFoundMessage);
   vsprintf(buf, noRecordsFoundMessage.c_str(), args);
   va_end(args);
   return CheckIfRecordsWereFound(sqlStatement, buf);
}
