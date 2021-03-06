#include <Arduino.h>
#include <Thread.h>
#include <ThreadController.h>
#include <TinyGPS++.h>
#include "debug.h"
#include "connection.h"
//#include "ota.h"
#include "serverclient.h"

#define LED D4

ESP8266WiFiGenericClass p;


TinyGPSPlus gps;


String gpsData;

char latitude[20];
char longitude[20];
char timegps[20];


String displayInfo()
 {
   String string;
   if (gps.location.isValid())
   {
            
    dtostrf(gps.location.lat(), 8, 7, latitude);
    dtostrf(gps.location.lng(), 8, 7, longitude);
    dtostrf(gps.time.value(), 8, 7, timegps);

    string.concat(timegps);
    string.concat(",\"data\":[");
    string.concat(latitude);
    string.concat(",");
    string.concat(longitude);
    

   }
   else
   {

    string.concat("00000000");
    string.concat(",\"data\":[");
    string.concat("0.0");
    string.concat(",");
    string.concat("0.0");

     Serial.print(F("INVALID"));
   }
   
  return string;
 }



void hardwareInit()
{
    Serial.begin(115200);


    Serial.print(F("Testing TinyGPS++ library v. ")); Serial.println(TinyGPSPlus::libraryVersion());
    debug("Starting Hardware.");
    pinMode(LED, OUTPUT);
}

Thread printDataThread = Thread();
Thread connectionThread = Thread();
Thread serverClientThread = Thread();
ThreadController groupOfThreads = ThreadController();

//SETUP
void setup()
{

    p.setOutputPower(0);

    hardwareInit();

    //Start data thread
    Data::self();
    printDataThread.onRun([]() {
        Data::self().setGPSString(gpsData);
        const static unsigned long startTime = micros();
        Data::self().data->freeHeap = ESP.getFreeHeap();
        Data::self().data->upTime = (micros() - startTime)/1e3;
        Data::self().print();
    });
    /* The data thread must run twice the serverClientThread because
     * of Json memory problem, when running serverClientThread::run
     * json data get lost
     */
    printDataThread.setInterval(100);

    //Start connection manager thread
    Connection::self();
    ServerClient::self();
    Connection::self().setData(Data::self().data);

        debug("Master Mode");
        const static char* apname = "provant";
        const static IPAddress ip(192, 168, 0, 1 );
        const static IPAddress gateway(192, 168, 1, 1);
        const static IPAddress subnet(255, 255, 255, 0);
        Connection::self().wifi()->softAPConfig(ip, gateway, subnet);
        Connection::self().wifi()->softAP(apname);
        ServerClient::self().setServer();

    connectionThread.onRun([]() {
        if(Connection::self().run()) {
            digitalWrite(LED, !digitalRead(LED));
        }
    });
    connectionThread.setInterval(10);

    //Start serverClient thread
    ServerClient::self().begin();
    serverClientThread.onRun([]() {
        ServerClient::self().setData(Data::self().data);
        ServerClient::self().run();
    });
    //if(ServerClient::self().isServer())
        serverClientThread.setInterval(100);
    //else
    //    serverClientThread.setInterval(1);

    //Add all threads in ThreadController
    groupOfThreads.add(&printDataThread);
    groupOfThreads.add(&connectionThread);
    groupOfThreads.add(&serverClientThread);

}

//LOOP
void loop()
{
  groupOfThreads.run();
    //BEGIN GPS READ

  while (Serial.available() > 0)
    if (gps.encode(Serial.read()))
    gpsData = displayInfo();

  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while(true);
  }

  //END GPS READ

  yield();
}


