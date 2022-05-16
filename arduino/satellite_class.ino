#include <Regexp.h>

//according to scintific paper: 10.1016/j.chinastron.2009.12.009
//average error of SGP4/SDP4 model makes up 1.3 km per day
const double kErrorKmPerSecond = 1.3 / 86400.0;
const int kDiameterOfNearEarthOrbit = 12800;

String fromUnixtimeToMessage(time_t unixtime);

class SmartCharPtr {
  public:
    SmartCharPtr(size_t _size);
    SmartCharPtr(const String& str);
    SmartCharPtr(const SmartCharPtr& _other) = delete;
    SmartCharPtr(SmartCharPtr&& _other) = delete;

    SmartCharPtr& operator=(const SmartCharPtr& _other) = delete;
    SmartCharPtr& operator=(SmartCharPtr&& _other) = delete;
    char* operator*() const;

    ~SmartCharPtr();

  private:
    char* ptr;
};


Satellite::Satellite()
  : assigned_flag(false), error(-1) {};

bool Satellite::set_identity(const JsonObject& json, String& error_message) {
  for (String i : {"tle1", "tle2"}) {
    if (!json.containsKey(i)) {
      error_message = "there is no required parameter " + i;
      return false;
    }
  }

  if (!set_name(
        json.containsKey("name") ?
        json["name"].as<String>()
        : ""
      )) {
    error_message = "name is longer than 25 symbols";
    return false;
  }

  if (!set_tle1(json["tle1"].as<String>())) {
    error_message = "tle1 has a non-valid format";
    return false;
  }
  if (!set_tle2(json["tle2"].as<String>())) {
    error_message = "tle2 has a non-valid format";
    return false;
  }
  
  assigned_flag = true;
  error = -1;
  sgp4.site(station.latitude, station.longitude, station.altitude);
  sgp4.init(*SmartCharPtr{satellite_name},
            *SmartCharPtr{tle1},
            *SmartCharPtr{tle2});
  
  find_position();
  return true;
}

String Satellite::get_identity_message() const {
  String message;
  message += "name: " + satellite_name + '\n';
  message += "tle1: " + tle1 + '\n';
  message += "tle2: " + tle2;

  return message;
}

String Satellite::get_position_message() const {
  int _min, _hr, _day, _mon, _year;
  double _sec;
  invjday(sgp4.satJd , -1, true, _year, _mon, _day, _hr, _min, _sec);
  String message = "Tle measurement time: " + fromUnixtimeToMessage(get_epoch());
  message += "\nTime of sgp4 position measurement: ";
  message += fillZeroes(_hr) + ':' + fillZeroes(_min) + ':' + fillZeroes(int(_sec)) + ' ';
  message += fillZeroes(_day) + '.' + fillZeroes(_mon) + '.' + String(_year) + "\n";

  message += String(int((last_measurement_time - get_epoch()) / 86400.0)) + " days have passed since the tle measurement.\n";
  if (error >= kDiameterOfNearEarthOrbit) {
    message += "Position measurement is not valid, you should update your TLE data.\n\n";
  }
  else {
    message += "Error makes up around " + String(error) + " km.\n\n";
  }
  
  message += "azimuth = " + String(sgp4.satAz) + " elevation = " + String(sgp4.satEl) + " distance = " + String(sgp4.satDist) + '\n';
  message += "latitude = " + String(sgp4.satLat) + " longitude = " + String(sgp4.satLon) + " altitude = " + String(sgp4.satAlt) + '\n';

  message += get_horizontal_angle() < 0 || get_vertical_angle() < 0 ? "Under horizon" : "Visible now";

  return message;
}

String Satellite::get_properties_json() const {
  
  DynamicJsonDocument doc(1024);

  doc["is_satellite_allowed"] = false;
  doc["satellite_epoch"] = "&lt;not assigned&gt;";
  doc["satellite_measurement_time"] = "&lt;not assigned&gt;";
  doc["satellite_coords"] = "&lt;not assigned&gt;";
  doc["satellite_distance"] = "&lt;not assigned&gt;";
  doc["satellite_vision_status"] = "&lt;not assigned&gt;";
  doc["satellite_name"] = "&lt;not assigned&gt;";
  doc["satellite_error"] = "&lt;not assigned&gt;";
  doc["station_angle_fix"] = String(station.horizontal_correction) + "°, " + String(station.vertical_correction) + "°";

  if (was_assigned()) {
    doc["is_satellite_allowed"] = true;
    int _min, _hr, _day, _mon, _year;
    double _sec;
    invjday(sgp4.satJd , -1, true, _year, _mon, _day, _hr, _min, _sec);
    doc["satellite_measurement_time"] = String(_year) + '-' + fillZeroes(_mon) + '-' + fillZeroes(_day) + 'T' \
                                                + fillZeroes(_hr) + ':' + fillZeroes(_min) + ':' + fillZeroes(int(_sec)) + 'Z';
    doc["satellite_name"] = satellite_name;
    doc["satellite_coords"] =  String(sgp4.satLat) + "°, " + String(sgp4.satLon) + "°, " + String(sgp4.satAlt) + 'm';
    doc["satellite_distance"] = String(sgp4.satDist) + 'm';
    //doc["satellite_vision_status"] = get_horizontal_angle() < 0 || get_vertical_angle() < 0 ? "Under horizon" : "Visible now";
    doc["satellite_vision_status"] = sgp4.satVis == -2 ? "Under horizon" : "Visible now";

   doc["satellite_epoch"] = String(get_epoch());
   if (error < kDiameterOfNearEarthOrbit) {
      doc["satellite_error"] = String(error) + " km";
   } else {
      doc["satellite_error"] = "∞ km, you should update your TLE data.";
   }
  }

  doc["station_coords"] = String(station.latitude) + "°, " + String(station.longitude) + "°, " + String(station.altitude) + 'm';
  doc["station_time"] = now();
  
  String json_string;
  serializeJson(doc, json_string);
  
  return json_string;
}

double Satellite::get_horizontal_angle() const {
  if (was_assigned()) {
    return sgp4.satAz + station.horizontal_correction;
  }
  return 0;
}

double Satellite::get_vertical_angle() const {
  if (was_assigned()) {
    return sgp4.satEl + station.vertical_correction;
  }
  return 0;
}

bool Satellite::was_assigned() const {
  return assigned_flag;
}

void Satellite::find_position() {
  last_measurement_time = now();
  sgp4.findsat((unsigned long) {
    last_measurement_time
  });
  error = (last_measurement_time - get_epoch()) * kErrorKmPerSecond;
}

bool Satellite::set_name(const String& new_name) {
  if (new_name.length() > 25) {
    return false;
  }

  satellite_name =
    new_name != ""
    ? new_name
    : "<unnamed>";
  return true;
}

bool Satellite::set_tle1(const String& new_tle1) {
  MatchState ms;
  auto c_str = SmartCharPtr{new_tle1};
  // 1 26400U 00037A   00208.84261022 +.00077745 +00000-0 +63394-3 0  9995
  // 1 25544U 98067A   08264.51782528 -.00002182  00000-0 -11606-4 0  2927
  ms.Target(*c_str);
  char match_result = ms.Match ("(1) (%d%d%d%d%d)([US ]) (%d%d)(%d%d%d)([A-Z ]?[A-Z ]?[A-Z ]) (%d%d)(%d...........) ([-+ ].%d%d%d%d%d%d%d%d) ([-+ ]%d%d%d%d%d[-+ ]%d) ([-+ ]%d%d%d%d%d[-+]%d) ([0-4]) ([ %d][ %d][ %d][ %d])(%d)");
  if (match_result > 0) {
    tle1 = new_tle1;
    return true;
  }
  Serial.println(new_tle1);
  return false;
}

bool Satellite::set_tle2(const String& new_tle2) {
  MatchState ms;
  auto c_str = SmartCharPtr{new_tle2};
  ms.Target(*c_str);
  char match_result = ms.Match ("(2) (%d%d%d%d%d) ([ %d][ %d][ %d].[%d ][%d ][%d ][%d ]) ([ %d][ %d][ %d].[ %d][ %d][ %d][ %d]) (%d%d%d%d%d%d%d) ([ %d][ %d][ %d].[%d ][%d ][%d ][%d ]) ([ %d][ %d][ %d].[%d ][%d ][%d ][%d ]) ([ %d][ %d].[%d ][%d ][%d ][%d ][%d ][%d ][%d ][%d ])([ %d][ %d][ %d][ %d][ %d])(%d)");
  if (match_result > 0) {
    tle2 = new_tle2;
    return true;
  }
  Serial.println(new_tle2);
  return false;
}

time_t Satellite::get_epoch() const {
  int _min, _hr, _day, _mon, _year;
  double _sec;
  invjday(sgp4.satrec.jdsatepoch, -1, true, _year, _mon, _day, _hr, _min, _sec);
  return makeTime({int(_sec), _min, _hr, 0, _day, _mon, _year - 1970});
}

SmartCharPtr::SmartCharPtr(size_t _size)
  : ptr(new char[_size]) {};

SmartCharPtr::SmartCharPtr(const String& str)
  : ptr(new char[str.length() + 1]) {
  str.toCharArray(ptr, str.length() + 1);
}

char* SmartCharPtr::operator*() const {
  return ptr;
}

SmartCharPtr::~SmartCharPtr() {
  delete[] ptr;
}
