const int kTleRecieveTimeout = 20000;
const int kTimeRecieveTimeout = 5000;

static const char kNtpServerName[] = "us.pool.ntp.org";
String kTleServerName = "http://tle-server.herokuapp.com";

void sendNtpPacket(IPAddress&, byte*, const int);

time_t getTimeDefault() {
  return 0;
}

time_t getNtpTime() {
  IPAddress ntp_server_ip; // NTP server's ip address
  const int ntp_packet_size = 48; // NTP time is in the first 48 bytes of message
  byte packet_buffer[ntp_packet_size];

  while (udp.parsePacket() > 0) ; // discard any previously received packets
  // get a random server from the pool
  WiFi.hostByName(kNtpServerName, ntp_server_ip);
  sendNtpPacket(ntp_server_ip, packet_buffer, ntp_packet_size);
  uint32_t begin_wait = millis();
  while (millis() - begin_wait < kTimeRecieveTimeout) {
    int size = udp.parsePacket();
    if (size >= ntp_packet_size) {
      udp.read(packet_buffer, ntp_packet_size);  // read packet into the buffer
      unsigned long secs_since_1900;
      // convert four bytes starting at location 40 to a long integer
      secs_since_1900 =  (unsigned long)packet_buffer[40] << 24;
      secs_since_1900 |= (unsigned long)packet_buffer[41] << 16;
      secs_since_1900 |= (unsigned long)packet_buffer[42] << 8;
      secs_since_1900 |= (unsigned long)packet_buffer[43];
      station.set_internet_availability(true);
      Serial.println("current unixtime: " + String(secs_since_1900 - 2208988800UL));
      return secs_since_1900 - 2208988800UL;
    }
  }
  Serial.println("No NTP Response :-(\n");
  station.set_internet_availability(false);
  return 0;
}

// send an NTP request to the time server at the given address
void sendNtpPacket(IPAddress& address, byte* packet_buffer, const int ntp_packet_size) {
  // set all bytes in the buffer to 0
  memset(packet_buffer, 0, ntp_packet_size);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packet_buffer[0] = 0b11100011;   // LI, Version, Mode
  packet_buffer[1] = 0;     // Stratum, or type of clock
  packet_buffer[2] = 6;     // Polling Interval
  packet_buffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packet_buffer[12] = 49;
  packet_buffer[13] = 0x4E;
  packet_buffer[14] = 49;
  packet_buffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123);
  udp.write(packet_buffer, ntp_packet_size);
  udp.endPacket();
}

String getTleFromTleServer(const String& arguments) {
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(kTleRecieveTimeout);
  String url = kTleServerName + "/get/tle?" + arguments;
  http.begin(client, url.c_str());

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    if (httpResponseCode == 200) {
      client.stop();
      http.end();
      return payload;
    }
    Serial.println("\nError code: " + String(httpResponseCode) + '\n');
    Serial.println("\npayload: " + payload + '\n');
  } else {
    Serial.print("\nError code: ");
    Serial.println(String(httpResponseCode) + '\n');
  }
  // Free resources
  client.stop();
  http.end();
  return "";
}
