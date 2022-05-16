Station::Station()
  : latitude(0), 
    longitude(0), 
    altitude(0), 
    horizontal_correction(0), 
    vertical_correction(0), 
    is_internet_available(false) {}

bool Station::set_latitude(double new_latitude, String& error_message) {
  if (abs(new_latitude) > 90) {
    error_message = "absolute latitude value is more than 90 degrees, it is not set\n";
    return false;
  }
  latitude = new_latitude;
  satellite.sgp4.site(latitude, longitude, altitude);
  return true;
}

bool Station::set_longitude(double new_longitude, String& error_message) {
  if (abs(new_longitude) > 180) {
    error_message = "absolute longitude value is more than 180 degrees, it is not set\n";
    return false;
  }
  longitude = new_longitude;
  satellite.sgp4.site(latitude, longitude, altitude);
  return true;
}

void Station::set_altitude(double new_altitude) {
  altitude = new_altitude;
  satellite.sgp4.site(latitude, longitude, altitude);
}

void Station::set_internet_availability(bool new_status) {
  if (is_internet_available == true && new_status == false) {
    setSyncProvider(getTimeDefault);
  }
  is_internet_available = new_status;
}

bool Station::set_horizontal_correction(double new_correction, String& error_message) {
  if (abs(new_correction) >= 180) {
    error_message = "absolute correction of horizontal angle is more than 180 degrees, it is not set\n";
    return false;
  }
  horizontal_correction = new_correction;
  return true;
}

bool Station::set_vertical_correction(double new_correction, String& error_message) {
  if (abs(new_correction) >= 180) {
    error_message = "absolute correction of vertical angle is more than 180 degrees, it is not set\n";
    return false;
  }
  vertical_correction = new_correction;
  return true;
}

String Station::get_info_message() {
  String message;
  String internet_connection_flag = is_internet_available ? "true" : "false";
  message += "Availability of internet connection: " + internet_connection_flag + '\n';
  message += "Station latitude " + String(latitude, 7) + '\n';
  message += "Station longitude " + String(longitude, 7) + '\n';
  message += "Station altitude " + String(altitude, 2) + '\n';
  message += "Horizontal correction " + String(horizontal_correction, 2) + '\n';
  message += "Vertical correction " + String(vertical_correction, 2);
  return message;
}
