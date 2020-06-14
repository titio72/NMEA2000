/* 
 * File:   main.cpp
 * Author: aboni
 */

#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <math.h>
#include <time.h>
#include <unistd.h>

char g_gps_port_name[] = "                       ";
char g_socket_can_name[] = "                        ";
#define SOCKET_CAN_PORT g_socket_can_name
#include "NMEA2000_CAN.h"
#include "NMEA2000.h"
#include "N2kMessages.h"
#include "Ports.h"
#include "NMEA.h"

#define TRACE

using namespace std;

uint g_valid_rmc = 0;
uint g_valid_gsa = 0;

uint ing_valid_rmc = 0;
uint ing_valid_gsa = 0;

uint g_pos_sent = 0;
uint g_pos_sent_fail = 0;

time_t g_last_time_report = 0;

RMC g_rmc;
GSA g_gsa;

#define OK 0
#define KO -1


void sendCOGSOG()
{
    if (g_gsa.valid && g_rmc.valid && g_gsa.fix >= 2 /*2=2dFix, 3=3dFix*/)
    {
        if (isnan(g_rmc.sog) && isnan(g_rmc.cog)) return;
        else
        {
            tN2kMsg N2kMsg;
            SetN2kCOGSOGRapid(N2kMsg, 1, N2khr_true, DegToRad(isnan(g_rmc.cog)?0.0:g_rmc.cog), g_rmc.sog * 1852.0 / 3600);
            NMEA2000.SendMsg(N2kMsg);
        }
    }
}

void sendTime()
{
    if (g_gsa.valid && g_rmc.valid && g_gsa.fix >= 2 /*2=2dFix, 3=3dFix*/)
    {
        tN2kMsg N2kMsg;
        int days_since_1970 = NMEAUtils::getDaysSince1970(g_rmc.y, g_rmc.M, g_rmc.d);
        double second_since_midnight = g_rmc.h * 60 * 60 + g_rmc.m * 60 + g_rmc.s + g_rmc.ms / 1000.0;
        SetN2kSystemTime(N2kMsg, 1, days_since_1970, second_since_midnight);
        NMEA2000.SendMsg(N2kMsg);
    }
}

void sendPosition()
{
    if (g_gsa.valid && g_rmc.valid && g_gsa.fix >= 2 /*2=2dFix, 3=3dFix*/)
    {
        tN2kMsg N2kMsg;
        SetN2kPGN129025(N2kMsg, g_rmc.lat, g_rmc.lon);
        bool sent = NMEA2000.SendMsg(N2kMsg);
        if (sent)
            g_pos_sent++;
    }
}

int parse_and_send(const char *sentence) {
    if (NMEAUtils::is_sentence(sentence, "RMC")) {
        if (NMEAUtils::parseRMC(sentence, g_rmc) == 0)
        {
            if (g_rmc.valid)
                g_valid_rmc++;
            else
                ing_valid_rmc++;

#ifdef TRACE
            printf("Process RMC {%s}\n", sentence);
            static char buffer[256];
            NMEAUtils::dumpGSA(g_gsa, buffer);
            printf("%s\n", buffer);
            NMEAUtils::dumpRMC(g_rmc, buffer);
            printf("%s\n", buffer);
#endif

            sendCOGSOG();
            NMEA2000.ParseMessages();
            sendTime();
            NMEA2000.ParseMessages();
            sendPosition();
            NMEA2000.ParseMessages();
            return OK;
        }
    } else if (NMEAUtils::is_sentence(sentence, "GSA")) {
        if (NMEAUtils::parseGSA(sentence, g_gsa) == 0)
        {
            if (g_gsa.valid)
                g_valid_gsa++;
            else
                ing_valid_gsa++;
            return OK;
        }
    }

    time_t t = time(0);

    if ((t - g_last_time_report) > 30)
    {
        g_last_time_report = time(0);
#ifdef TRACE
        printf("RMC %d/%d GSA %d/%d Sats %d Fix %d Sent %d/%d\n",
               g_valid_rmc, ing_valid_rmc,
               g_valid_gsa, ing_valid_gsa,
               g_gsa.nSat, g_gsa.fix,
               g_pos_sent, g_pos_sent_fail);
#endif
        g_valid_rmc = 0;
        ing_valid_rmc = 0;
        g_valid_gsa = 0;
        ing_valid_gsa = 0;
        g_pos_sent = 0;
        g_pos_sent_fail = 0;
    }

    return KO;
}

void setup()
{
    // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega
    NMEA2000.SetN2kCANSendFrameBufSize(250);

    // Set Product information
    NMEA2000.SetProductInformation("00000001",              // Manufacturer's Model serial code
                                   100,                     // Manufacturer's product code
                                   "ABONI GPS",             // Manufacturer's Model ID
                                   "1.0.2.25 (2019-07-07)", // Manufacturer's Software version code
                                   "1.0.2.0 (2019-07-07)"   // Manufacturer's Model version
    );
    // Set device information
    NMEA2000.SetDeviceInformation(1,   // Unique number. Use e.g. Serial number.
                                  132, // Device function=Analog to NMEA 2000 Gateway. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                  25,  // Device class=Inter/Intranetwork Device. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                  2046 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
    );

    NMEA2000.SetForwardStream(&serStream); // PC output on due programming port
    NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly, 22);
    NMEA2000.SetForwardType(tNMEA2000::fwdt_Text); // Show in clear text. Leave uncommented for default Actisense format.
    //NMEA2000.SetForwardOwnMessages();
    NMEA2000.EnableForward(false); // Disable all msg forwarding to USB (=Serial)
    NMEA2000.Open();
}

void sleepms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void load_conf()
{
    // set the defaults
    strcpy(g_socket_can_name, "vcan0");
    strcpy(g_socket_can_name, "vcan0");

    FILE *pFile = fopen("/etc/nmea2000.conf", "r");
    if (pFile == NULL)
    {
        pFile = fopen("./nmea2000.conf", "r");
    }
    if (pFile)
    {
        // obtain file size:
        fseek(pFile, 0, SEEK_END);
        size_t lSize = ftell(pFile);
        rewind(pFile);
        char *buffer = new char[lSize + sizeof(char)];
        size_t r = fread(buffer, 1, lSize, pFile);
        if (r == lSize)
        {
            char *conf = buffer;
            char *token = strsep(&conf, "\n");
            if (token && token[0])
            {
                strcpy(g_gps_port_name, token);
#ifdef TRACE
                printf("[%s]", token);
#endif
            }
            token = strsep(&conf, "\n");
            if (token && token[0])
            {
                strcpy(g_socket_can_name, token);
#ifdef TRACE
                printf(" [%s]\n", token);
#endif
            }
        }
        fclose(pFile);
    }
}

int main(int argc, char *argv[])
{
    load_conf();
    memset(&g_gsa, 0, sizeof(GSA));
    memset(&g_rmc, 0, sizeof(RMC));

    if (argc>1 && strcmp("sim", argv[1])==0) {
        printf("Setting up CAN BUS\n");
        setup();
        printf("Start simulation\n");
        while (1) {
            g_gsa.fix = 3;
            g_gsa.hdop = 1.2;
            g_gsa.nSat = 6;
            g_gsa.valid = 1;

            g_rmc.cog = 271.0;
            g_rmc.sog = 7.1;
            g_rmc.lat = 44.0666656;
            g_rmc.lon = 9.8166666;
            g_rmc.valid = 1;
            time_t t = time(0);
            tm* tt = gmtime(&t);
            g_rmc.y = tt->tm_year + 1900;
            g_rmc.M = tt->tm_mon + 1;
            g_rmc.d = tt->tm_mday;
            g_rmc.h = tt->tm_hour;
            g_rmc.m = tt->tm_min;
            g_rmc.s = tt->tm_sec;
            g_rmc.ms = 0;
            free(tt);
            sendTime();
            sendCOGSOG();
            sendPosition();
            sleepms(500);
        }
    } else {
        setup();
        if (strcmp("-", g_gps_port_name)==0) {
            
            g_gsa.fix = 3;
            g_gsa.hdop = 1.2;
            g_gsa.nSat = 6;
            g_gsa.valid = 1;

            printf("Reading from STDIN\n");
            static char buffer[2048];
            int pos = 0;
            buffer[pos] = 0;
            char ch;
            while(1) {
                if (read(STDIN_FILENO, &ch, 1) > 0) {
                    if (ch==13 or ch==10) {
                        if (buffer[0]) parse_and_send(buffer);
                        pos = 0;
                        buffer[pos] = 0;
                    } else {
                        buffer[pos] = ch;
                        pos++;
                        buffer[pos] = 0;
                    }
                } else {
                    sleepms(100);
                }
            }
        } else {
            Port p(g_gps_port_name);
            p.set_handler(parse_and_send);
            p.listen();
        }
    }
}
