/* 
 * File:   NMEA.h
 * Author: aboni
 */

#ifndef NMEA_H_
#define NMEA_H_

struct GSA
{
    short valid;
    int nSat;
    int fix;
    float hdop;
    float vdop;
    float pdop;
};

struct RMC
{
    short valid;
    float lat;
    float lon;

    short y;
    short M;
    short d;
    short h;
    short m;
    short s;
    short ms;

    float cog;
    float sog;
};

class NMEAUtils
{
public:
    static int getDaysSince1970(int y, int m, int d);
    static int parseRMC(const char *s_rmc, RMC &rmc);
    static int parseGSA(const char *s_rmc, GSA &gsa);
    static void dumpGSA(GSA &gsa, char *buffer);
    static void dumpRMC(RMC &rmc, char *buffer);
    static bool is_sentence(const char* sentence, const char* sentence_id);
};

#endif