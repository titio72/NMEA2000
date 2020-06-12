/* 
 * File:   NMEA.cpp
 * Author: aboni
 */

#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <math.h>
#include "NMEA.h"

// To store number of days in all months from January to Dec.
const int monthDays[12] = {31, 28, 31, 30, 31, 30,
                           31, 31, 30, 31, 30, 31};

int countLeapYears(int year, int month)
{
    // Check if the current year needs to be considered
    // for the count of leap years or not
    if (month <= 2)
        year--;

    // An year is a leap year if it is a multiple of 4,
    // multiple of 400 and not a multiple of 100.
    return year / 4 - year / 100 + year / 400;
}

int nmea_position_parse(char *s, float *pos)
{
    char *cursor;

    int degrees = 0;
    float minutes = 0;

    if (s == NULL || *s == '\0')
    {
        return -1;
    }

    /* decimal minutes */
    if (NULL == (cursor = strchr(s, '.')))
    {
        return -1;
    }

    /* minutes starts 2 digits before dot */
    cursor -= 2;
    minutes = atof(cursor);
    *cursor = '\0';

    /* integer degrees */
    cursor = s;
    degrees = atoi(cursor);

    *pos = degrees + (minutes / 60.0);

    return 0;
}

int NMEAUtils::getDaysSince1970(int y, int m, int d)
{
    // COUNT TOTAL NUMBER OF DAYS BEFORE FIRST DATE 'dt1'

    // initialize count using years and day
    long int n1 = 1970 * 365 + 1;

    // Since every leap year is of 366 days,
    // Add a day for every leap year
    n1 += countLeapYears(1970, 1);

    // SIMILARLY, COUNT TOTAL NUMBER OF DAYS BEFORE 'dt2'

    long int n2 = y * 365 + d;
    for (int i = 0; i < m - 1; i++)
        n2 += monthDays[i];
    n2 += countLeapYears(y, m);

    // return difference between two counts
    return (n2 - n1);
}

int NMEAUtils::parseGSA(const char *s_gsa, GSA &gsa)
{
    char *tofree = strdup(s_gsa);
    char *tempstr = tofree;
    char *token = strsep(&tempstr, ",");
    if (strlen(token) && strcmp(token + sizeof(char) * 3, "GSA") == 0)
    {
        memset(&gsa, 0, sizeof(GSA));

        gsa.valid = 1;

        // read mode
        token = strsep(&tempstr, ",");

        // read fix
        token = strsep(&tempstr, ",");
        gsa.fix = atoi(token);

        // count sats
        gsa.nSat = 0;
        for (int i = 0; i < 12; i++)
        {
            token = strsep(&tempstr, ",");
            if (token && token[0])
                gsa.nSat++;
        }

        // read PDOP
        token = strsep(&tempstr, ",");
        gsa.pdop = atof(token);

        // read HDOP
        token = strsep(&tempstr, ",");
        gsa.hdop = atof(token);

        // read VDOP
        token = strsep(&tempstr, ",");
        gsa.vdop = atof(token);

        free(tofree);
        return 0;
    }
    free(tofree);
    return -1;
}

void NMEAUtils::dumpRMC(RMC &rmc, char *buffer)
{
    sprintf(buffer, "RMC valid {%d} lat {%.4f} lon {%.4f} cog {%.1f} sog {%.2f} time {%d-%d-%dT%d:%d:%dZ}",
            rmc.valid, rmc.lat, rmc.lon, rmc.cog, rmc.sog, rmc.y, rmc.M, rmc.d, rmc.h, rmc.m, rmc.s);
}

void NMEAUtils::dumpGSA(GSA &gsa, char *buffer)
{
    sprintf(buffer, "RSA valid {%d} nSat {%d} fix {%d} HDOP {%.1f}", gsa.valid, gsa.nSat, gsa.fix, gsa.hdop);
}

int NMEAUtils::parseRMC(const char *s_rmc, RMC &rmc)
{
    char *tofree = strdup(s_rmc);
    char *tempstr = tofree;
    char *token = strsep(&tempstr, ",");
    if (strlen(token) && strcmp(token + sizeof(char) * 3, "RMC") == 0)
    {
        memset(&rmc, 0, sizeof(RMC));

        // read UTC time
        token = strsep(&tempstr, ",");
        if (strlen(token) > 6)
        {
            token[6] = 0;
            rmc.ms = atoi(token + 7 * sizeof(char));
        }
        rmc.s = atoi(token + 4 * sizeof(char));
        token[4] = 0;
        rmc.m = atoi(token + 2 * sizeof(char));
        token[2] = 0;
        rmc.h = atoi(token);

        // read validity
        token = strsep(&tempstr, ",");

        // read latitude
        token = strsep(&tempstr, ",");
        rmc.lat = NAN;
        nmea_position_parse(token, &(rmc.lat));
        token = strsep(&tempstr, ",");
        if (!isnan(rmc.lat) && token && token[0])
        {
            if (token[0] == 'S')
                rmc.lat = -rmc.lat;
            rmc.valid = 1;
        }

        // read longitude
        token = strsep(&tempstr, ",");
        rmc.lon = NAN;
        nmea_position_parse(token, &(rmc.lon));
        token = strsep(&tempstr, ",");
        if (!isnan(rmc.lon) && token && token[0])
        {
            if (token[0] == 'W')
                rmc.lon = -rmc.lon;
        }
        else
        {
            rmc.valid = 0;
        }

        // read SOG in knots
        token = strsep(&tempstr, ",");
        rmc.sog = (token && token[0]) ? atof(token) : NAN;

        // read COG in degrees
        token = strsep(&tempstr, ",");
        rmc.cog = (token && token[0]) ? atof(token) : NAN;

        // read date
        token = strsep(&tempstr, ",");
        rmc.y = atoi(token + 4 * sizeof(char)) + 2000;
        token[4] = 0;
        rmc.M = atoi(token + 2 * sizeof(char));
        token[2] = 0;
        rmc.d = atoi(token);

        free(tofree);
        return 0;
    }
    free(tofree);
    return -1;
}
