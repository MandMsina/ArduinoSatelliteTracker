#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <TimeLib.h>

#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Servo.h>
#include <Sgp4.h>

#include <ArduinoJson.h>

namespace wifi {
const char* kSsid = "Covid-19-5G Test Tower 182.18.1";
const char* kPassword = "IamFeoktistov";
//const char* kSsid = "netis_2.4G_2E00E7";
//const char* kPassword = "password";
} // namespace wifi

class Satellite {
  public:
    friend class Station;
    Satellite();
    bool set_identity(const JsonObject& json, String& error_message);
    String get_identity_message() const;
    String get_position_message() const;
    String get_properties_json() const;

    double get_horizontal_angle() const;
    double get_vertical_angle() const;
    
    bool was_assigned() const;
    void find_position();

  private:
    Sgp4 sgp4;
    String satellite_name;
    String tle1;
    String tle2;
    time_t last_measurement_time;
    
    double error;
    bool assigned_flag;

    bool set_name(const String& new_name);
    bool set_tle1(const String& new_tle1);
    bool set_tle2(const String& new_tle2);

    time_t get_epoch() const;
};

class Station {
  public:
    friend class Satellite;
    Station();
    bool set_latitude(double new_latitude, String& error_message);
    bool set_longitude(double new_longitude, String& error_message);
    void set_altitude(double new_altitude);
    void set_internet_availability(bool new_status);
    bool set_horizontal_correction(double new_correction, String& error_message);
    bool set_vertical_correction(double new_correction, String& error_message);

    String get_info_message();

  private:
    double latitude;
    double longitude;
    double altitude;

    double horizontal_correction;
    double vertical_correction;
    
    bool is_internet_available;
};

ESP8266WebServer server(80);
WiFiUDP udp;
Servo horizontal;
Servo vertical;
Satellite satellite;
Station station;

time_t getNtpTime();
time_t getTimeDefault();

String getTleFromTleServer(const String&);

namespace handlers {

void setStationCoordinates();
void setAnglesCorrection();
void setJsonTle();
void setServerTle();
void setUnixtime();
void setTimeFromServer();

void getSatelliteInfo();
void getStationInfo();

void apiRoot();

void htmlUpdate();
void htmlInfo();
void htmlRoot();

void notFound();

} // namespace handlers

void setup(void) {
  
  Serial.begin(115200);
  //WiFi.mode(WIFI_STA);
  WiFi.begin(wifi::kSsid, wifi::kPassword);
  Serial.println("");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to " + String(wifi::kSsid));
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("sat-tracker")) {
    Serial.println("MDNS responder started");
  }
  if (udp.begin(8888)) {
    Serial.println("Udp listener started");
  }

  setSyncProvider(getNtpTime);

  server.on("/api", handlers::apiRoot);
  
  server.on("/api/get/station-info", handlers::getStationInfo);
  server.on("/api/get/satellite-info", handlers::getSatelliteInfo);

  server.on("/api/set/coordinates", handlers::setStationCoordinates);
  server.on("/api/set/time", handlers::setUnixtime);
  server.on("/api/set/time/from-server", handlers::setTimeFromServer);
  server.on("/api/set/tle", handlers::setJsonTle);
  server.on("/api/set/tle/from-server", handlers::setServerTle);
  server.on("/api/set/angles", handlers::setAnglesCorrection);

  server.on("/update", handlers::htmlUpdate);
  server.on("/info", handlers::htmlInfo);
  server.on("/", handlers::htmlRoot);
  
  server.onNotFound(handlers::notFound);

  server.begin();
  Serial.println("HTTP server started");

  horizontal.attach(4, 600, 2400);  // attaches the servo on GIO2 to the servo object
  vertical.attach(5, 600, 2400);  // attaches the servo on GIO2 to the servo object
  vertical.write(0); 
  horizontal.write(0);
}

void loop(void) {
  if (satellite.was_assigned()) {
    satellite.find_position();
    horizontal_angle = satellite.get_horizontal_angle();
    vertical_angle = satellite.get_vertical_angle();
    if (vertical_angle > 0) {
      if (horizontal_angle > 180) {
        horizontal_angle = horizontal_angle - 180;
        vertical_angle = 180 - vertical_angle;
      }
      horizontal.write(horizontal_angle);
      vertical.write(vertical_angle);
    } else {
      vertical.write(0); 
      horizontal.write(0);
    }
  }
  server.handleClient();
  MDNS.update();
}
