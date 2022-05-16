String fromUnixtimeToMessage(time_t);
String fillZeroes(int, int n = 2);

namespace handlers {

void setJsonTle() {
  if (server.args() < 1) {
    server.send(400, "text/plain", "No json with tle-strings in body!\r\n");
  }
  String payload = server.arg(0);
  DynamicJsonDocument root(payload.length() * 20);
  DeserializationError err = deserializeJson(root, payload);
  if (err) {
    server.send(400, "text/plain", "Error of TLE parsing!\r\n");
  } else {
    String error_message;
    if (satellite.set_identity(root.as<JsonObject>(), error_message)) {
      server.send(200, "text/plain", "OK\n");
      Serial.println("\nNew satellite was specified!\n" + satellite.get_identity_message());
    }
    server.send(400, "text/plain", error_message + '\n');
  }

}

void setServerTle() {
  String arguments_to_request;
  for (uint8_t i = 0; i < server.args(); ++i) {
    if (server.argName(i) == "satname" || server.argName(i) == "norad_id" || server.argName(i) == "page") {
      if (arguments_to_request != "") {
        arguments_to_request += "&";
      }
      arguments_to_request += server.argName(i) + "=" + server.arg(i);
    } else {
      server.send(400, "text/plain", "There is no argument named '" + server.argName(i)
                  + "'.\nAvailable only 'satname' and 'norad_id'.\n");
      return;
    }
  }

  auto server_response = getTleFromTleServer(arguments_to_request);
  if (server_response == "") {
    server.send(500, "text/plain", "Something wents wrong with heroku server app!\n");
    return;
  }
  DynamicJsonDocument root(server_response.length() * 2);
  DeserializationError err = deserializeJson(root, server_response);
  switch (err.code()) {
    case DeserializationError::Ok:
      break;
    case DeserializationError::InvalidInput:
      Serial.print(F("Invalid input!\n"));
      break;
    case DeserializationError::NoMemory:
      Serial.print(F("Not enough memory\n"));
      break;
    case DeserializationError::TooDeep:
      Serial.print(F("To deep =( \n"));
      break;
    case DeserializationError::IncompleteInput:
      Serial.print(F("Incomplete input =( \n"));
      break;
    case DeserializationError::EmptyInput:
      Serial.print(F("Empty input =( \n"));
      break;
    default:
      Serial.print(F("Impossible result, only God can help us\n"));
      break;
  }
  if (err) {
    server.send(500, "text/plain", "Error of TLE parsing.\n");
    return;
  }

  if (root["empty_result"].as<bool>() == true) {
    server.send(200, "text/plain", "There is no satellite with this parameters. Please, change your request.\n");
    return;
  }

  if (root["is_only_one_result"].as<bool>() == false) {
    String message = "There are more than one satellite with this parameters. Please, change your request.\n\nFound results:\n";
    String page_number = root["page_number"].as<String>();
    String total_page_number = root["number_of_pages"].as<String>();
    for (const auto& satellite : root["list_of_variants"].as<JsonArray>()) {
      message += "satname: " + satellite["object_name"].as<String>() + ", norad_id: " + satellite["norad_id"].as<String>() + '\n';
    }
    message += "\n\nPage " + page_number + " of " + total_page_number + '.';
    server.send(200, "text/plain", message + "\nAdd argument 'page' to show other pages.\n");
  } else {
    String error_message;
    if (satellite.set_identity(root["satellite_identity"].as<JsonObject>(), error_message)) {
      server.send(200, "text/plain", "OK\n");
      Serial.println("\nNew satellite was specified!\n" + satellite.get_identity_message() + '\n');
    }
    server.send(400, "text/plain", error_message + '\n');
  }
}

void setUnixtime() {
  for (uint8_t i = 0; i < server.args(); ++i) {
    if (server.argName(i) == "unixtime") {
      double new_unixtime = atoi(server.arg(i).c_str());
      if (new_unixtime < 0) {
        server.send(400, "text/plain", "unixtime can't be negative\n");
        return;
      }
      setSyncProvider(getTimeDefault);
      setTime(new_unixtime);
      server.send(200, "text/plain", "OK\n");
      return;
    }
    else {
      server.send(400, "text/plain", "There is no argument named '" + server.argName(i)
                  + "'.\nAvailable only 'unixtime'.\n");
      return;
    }
  }
}

void setTimeFromServer() {
  auto current_time = getNtpTime();
  if (current_time) {
    setSyncProvider(getNtpTime);
    setTime(current_time);
    server.send(200, "text/plain", "OK\n");
  }
  server.send(500, "text/plain", "There is no internet connection\n");
}

void setStationCoordinates() {
  String error_message = "";
  for (uint8_t i = 0; i < server.args(); ++i) {
    if (server.argName(i) == "latitude") {
      station.set_latitude(atof(server.arg(i).c_str()), error_message);
    } else if (server.argName(i) == "longitude") {
      station.set_longitude(atof(server.arg(i).c_str()), error_message);
    } else if (server.argName(i) == "altitude") {
      station.set_altitude(atof(server.arg(i).c_str()));
    }
    else {
      server.send(400, "text/plain", "There is no argument named '" + server.argName(i)
                  + "'.\nAvailable only 'latitude', 'longitude' and 'altitude'.\n");
      return;
    }
  }
  if (error_message != "") {
    server.send(400, "text/plain", error_message);
  } else {
    server.send(200, "text/plain", "OK\n");
  }
}

void setAnglesCorrection() {
  String error_message = "";
  for (uint8_t i = 0; i < server.args(); ++i) {
    if (server.argName(i) == "vertical") {
      station.set_vertical_correction(atof(server.arg(i).c_str()), error_message);
    } else if (server.argName(i) == "horizontal") {
      station.set_horizontal_correction(atof(server.arg(i).c_str()), error_message);
    } else {
      server.send(400, "text/plain", "There is no argument named '" + server.argName(i)
                  + "'.\nAvailable only 'horizontal' and 'vertical'.\n");
      return;
    }
  }
  if (error_message != "") {
    server.send(400, "text/plain", error_message);
  } else {
    server.send(200, "text/plain", "OK\n");
  }
}

void getSatelliteInfo() {
  if (satellite.was_assigned()) {
    String message;
    message += satellite.get_identity_message() + "\n\n";
    message += satellite.get_position_message();

    server.send(200, "text/plain", message + '\n');
  }
  else {
    server.send(200, "text/plain", "No one satellite was speccified.\r\n");
  }
}

void getStationInfo() {
  String message;
  message += "System time: " + fromUnixtimeToMessage(now()) + '\n';
  message += station.get_info_message() + '\n';

  server.send(200, "text/plain", message + '\n');
}

void apiRoot() {
  String message;
  message += "Hello, dear friend!\n";
  message += "List of api handlers that you can use:\n\n";
  message += "'/api' - show this hello message\n\n";

  message += "'/api/get/station-info' - show current condition of navigation station\n";
  message += "'/api/get/satellite-info' - show info about currently chosen satellite\n\n";

  message += "'/api/set/coordinates' - set coordinates of station, available arguments: 'latitude', 'longitude', 'altitude'\n";
  message += "'/api/set/angles' - set correction of angles, available arguments: 'horizontal', 'vertical'";
  message += "'/api/set/time' - set initial time of station system, available arguments: 'unixtime'\n";
  message += "'/api/set/time/from-server' - set initial time of station system from server\n";
  message += "'/api/set/tle' - set new satellite tle data, required a json with fields: \n";
  message += "\t'tle1' - required parameter contained first tle string\n";
  message += "\t'tle2' - required parameter contained second tle string\n";
  message += "\t'name' - optional parameter contained satellite name\n";
  message += "'/api/set/tle/from-server' - set new satellite tle data from server, available arguments: \n";
  message += "\t'satname' - name of satellite (or part of it)\n";
  message += "\t'norad_id' - another way to get tle from server\n";
  message += "\t'page' - number of shown page (returned info will paging if more than 10 satellites from server contain 'satname')\n";

  server.send(200, "text/plain", message + '\n');
}

void htmlUpdate() {
  Serial.println("content was updated");
  if (server.args() < 1) {
    server.send(500, "text/plain", "{\"error\": \"No json with tle-strings in body!\"}");
    return;
  }
  String payload = server.arg(0);
  DynamicJsonDocument root(payload.length() * 2);
  DeserializationError err = deserializeJson(root, payload);
  if (err) {
    server.send(500, "text/plain", "{\"error\": \"Error of TLE parsing!\"}");
  } else {
    String method = root["method"].as<String>();
    if (method == "tle") {
      String error_message;
      if (satellite.set_identity(root["data"], error_message)) {
        server.send(200, "text/plain", satellite.get_properties_json());
        Serial.println("\nNew satellite was specified!\n" + satellite.get_identity_message() + '\n');
        return;
      }
      server.send(400, "text/plain", "{\"error\": \"" + error_message + "\"}");
    } else if (method == "current_time") {
      if (root["data"]["from_server"].as<bool>()) {
         auto current_time = getNtpTime();
         if (current_time) {
            setSyncProvider(getNtpTime);
            setTime(current_time);
            satellite.find_position();
            server.send(200, "text/plain", satellite.get_properties_json());
            return;
         }
         server.send(500, "text/plain", "{\"error\": \"There is no internet connection\"}");
      } else {
         setSyncProvider(getTimeDefault);
         setTime(atoi(root["data"]["unixtime"].as<char*>()));
         server.send(200, "text/plain", satellite.get_properties_json());
      }
    }
    else if (method == "coords") {
      String error_message = "";
      if (root["data"].containsKey("latitude")) {
        station.set_latitude(root["data"]["latitude"].as<double>(), error_message);
      } 
      if (root["data"].containsKey("longitude")) {
        station.set_longitude(root["data"]["longitude"].as<double>(), error_message);
      } 
      if (root["data"].containsKey("altitude")) {
        station.set_altitude(root["data"]["altitude"].as<double>());
      }
      if (error_message != "") {
        server.send(400, "text/plain", error_message);
      }
      server.send(200, "text/plain", satellite.get_properties_json());
    }
    else if (method == "angle_fix") {
      String error_message = "";
      if (root["data"].containsKey("elevation")) {
        station.set_vertical_correction(root["data"]["elevation"].as<double>(), error_message);
      } 
      if (root["data"].containsKey("azimuth")) {
        station.set_horizontal_correction(root["data"]["azimuth"].as<double>(), error_message);
      }
      if (error_message != "") {
        server.send(400, "text/plain", error_message);
      }
      server.send(200, "text/plain", satellite.get_properties_json());
    }
    else {
      server.send(500, "text/plain", "{\"error\": \"Method is not specified\"}");
    }
  }
}

void htmlInfo() {
  server.send(200, "text/plain", satellite.get_properties_json());
}

void htmlRoot() {
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Sattelite tracker by VladKrsk (v2.0)</title><link rel=\"preconnect\" href=\"https://fonts.googleapis.com\"><link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin><link href=\"https://fonts.googleapis.com/css2?family=Alegreya+SC&display=swap\" rel=\"stylesheet\"></head><style type=\"text/css\">body {background-color: rgb(22,22,22);background-image: url(\"https://habrastorage.org/webt/x0/qq/nw/x0qqnwcxscb4m-a6syf5kwgd16o.png\");background-size: 75%;background-position: top;background-repeat: no-repeat;color: #e3e3e3;font-family: 'Alegreya SC', serif;font-size: 1.5em;width:50%;padding-top: 8%;margin:auto auto;}div#exception_window{position: fixed;display: none;bottom: 0;right: 0;padding: 0;margin: 0;margin-bottom: 0.5em;margin-right: 0.5em;border: 0.2em solid #0f0f0f;font-family: Courier;font-size: 0.75em;max-width: 20%;max-height: 15%;}div#exception_window div {padding-left: 0.5em;padding-top: 0.25em;padding-bottom: 0.25em;padding-right: 0.5em;}div#exception_window #exc_code{background-color: #0f0f0f;}div {padding-top: 1.5em;width: 100%;display: inline-block;}.dropout , #auto_unixtime, #reload_data{background-color: rgba(0,0, 0, 0.3);font-family: 'Alegreya SC', serif;font-size: 1em;color: #e3e3e3;cursor: pointer;padding: 18px;width: 100%;text-align: left;border: none;outline: none;transition: 0.4s;}#auto_unixtime , #reload_data{padding: 0px;height:2em;width:auto;margin-top: 0.5em;margin-bottom: 0.5em;}#reload_data {width: 100%;text-align: center;}.dropout:hover , #auto_unixtime:hover, #reload_data:hover{background-color: rgb(33,33,33);}.manual_tle {padding-left:3%;font-size: 0.9em;width:75%;}/* Style the accordion panel. Note: hidden by default */.dropout_data {padding: 0px;margin: 0px;background-color: rgb(22,22,22);display: none;overflow: hidden;}input[type=text], textarea {background-color: rgb(22, 22, 22);color: #e3e3e3;font-family: 'Alegreya SC', serif;font-size: 1em;text-align: center;border-color: #383838;border-style: double;border-width: 0.25em;border-radius: 3px;height: 1.7em;}.optional_auto_tle td {width:48%;padding-right: 1%;}.optional_auto_tle{width: 100%;table-layout: fixed;}.image {padding-top:0px !important;opacity: 65%;display:block;margin-left: auto;margin-right: 5%;}.img {padding-top:0px !important;cursor: pointer;opacity: 65%;display:block;margin-left: auto;/*margin-right: 5%;*/}.img:hover {padding-top:0px !important;opacity: 100%;}table {display: inline-block;}td { height:4em;}td.key {width: 35%;}td.value {width: 50%;}td.submit{height:10%;width: 10%;}.current_info {width:67%;}ol {padding-left: 1em;margin-top: 0px;}</style><script>var b_start ='<svg width=\"43\" height=\"46\"';var b_end = '><rect x=\"0\" y=\"0\" width=\"42\" height=\"46\" stroke=\"#e3e3e3\" stroke-width=\"1\" fill=\"#161616\"/><rect x=\"2\" y=\"2\" width=\"38\" height=\"41\" stroke=\"#e3e3e3\" stroke-width=\"1\" fill=\"#161616\"/><polyline points=\"11,9 34,24 11,40\" style=\"fill:none;stroke:#e3e3e3;stroke-width:1\" /><polyline points=\"11,11 31,24 11,37\" style=\"fill:none;stroke:#e3e3e3;stroke-width:1\" /></svg>';var load_images_on_buttons = function() {document.getElementById(\"div_auto_tle_submit\").innerHTML = b_start + 'class=\"img\" id=\"auto_tle_submit\"' + b_end;document.getElementById(\"div_manual_tle_submit\").innerHTML = b_start + 'class=\"img\" id=\"manual_tle_submit\"' + b_end;document.getElementById(\"div_cur_time_submit\").innerHTML = b_start + 'class=\"img\" id=\"cur_time_submit\"' + b_end;document.getElementById(\"div_coords_submit\").innerHTML = b_start + 'class=\"img\" id=\"coords_submit\"' + b_end;document.getElementById(\"div_angle_fix_submit\").innerHTML = b_start + 'class=\"img\" id=\"angle_fix_submit\"' + b_end;};document.addEventListener('DOMContentLoaded', load_images_on_buttons, false);</script><script>var pagePagination = {code : '',Extend : function(data) {data = data || {};pagePagination.size = data.size || 300;pagePagination.page = data.page || 1;pagePagination.step = data.step || 3;},Add : function(p, q) {for (var l =p;l < q; l++) {pagePagination.code += '<a class=\"pageButton\">' + l + '</a>';}},Last : function() {pagePagination.code += '<i>...</i><a class=\"pageButton\">' + pagePagination.size + '</a>';},First : function() {pagePagination.code += '<a class=\"pageButton\">1</a><i>...</i>';},ClickPageNum : function() {pagePagination.page = +this.innerHTML;pagePagination.Start();},ClickTableOption : function() {document.getElementById('satname').value = this.getElementsByTagName('th')[0].innerHTML;document.getElementById('norad_id').value = this.id;autoTLESubmit();},Prev : function() {pagePagination.page--;if (pagePagination.page < 1) {pagePagination.page = 1;}pagePagination.Start();},Next : function() {pagePagination.page++;if (pagePagination.page > pagePagination.size) {pagePagination.page = pagePagination.size;}pagePagination.Start();},BindPageButtons : function() {var a = pagePagination.e.getElementsByClassName('pageButton');for (var num = 0; num < a.length; num++) {if (+a[num].innerHTML === pagePagination.page)a[num].className = 'current';a[num].addEventListener('click', pagePagination.ClickPageNum, false);}},BindOptions : function() {var options = document.getElementsByClassName('tableOption');for (var num = 0; num < options.length; num++) {options[num].addEventListener('click', pagePagination.ClickTableOption, false);}},Finish : function() {pagePagination.e.innerHTML = pagePagination.code;pagePagination.code = '';pagePagination.BindPageButtons();},Start : function() {pagePagination.Get_page(pagePagination.page);if (pagePagination.size < pagePagination.step * 2 + 6) {pagePagination.Add(1, pagePagination.size + 1);} else if (pagePagination.page < pagePagination.step * 2 + 1) {pagePagination.Add(1, pagePagination.step * 2 + 4);pagePagination.Last();} else if (pagePagination.page > pagePagination.size - pagePagination.step * 2) {pagePagination.First();pagePagination.Add(pagePagination.size - pagePagination.step * 2 - 2,pagePagination.size + 1);} else {pagePagination.First();pagePagination.Add(pagePagination.page - pagePagination.step,pagePagination.page + pagePagination.step + 1);pagePagination.Last();}pagePagination.Finish();},Buttons : function(e) {var nav = e.getElementsByClassName('goto');nav[0].addEventListener('click', pagePagination.Prev, false);nav[1].addEventListener('click', pagePagination.Next, false);},Create : function(e) {var html = [ '<a class=\"goto\">◄</a>','<span class=\"tablePagesButtons\"></span>','<a class=\"goto\">►</a>'];e.innerHTML = html.join('');pagePagination.e = e.getElementsByTagName('span')[0];pagePagination.Buttons(e);},Init : function(e, data, satname) {const xhr = new XMLHttpRequest();xhr.open(\"GET\", \"https://tle-server.herokuapp.com/get/tle?satname=\" + satname);xhr.responseType = 'json';xhr.onload = () => {data.size = xhr.response[\"number_of_pages\"];pagePagination.satname = satname;pagePagination.Extend(data);pagePagination.Create(e);pagePagination.Start();hide_loader();};xhr.send(null);},Hide : function() {document.getElementsByClassName('container')[0].style.visibility = 'collapse';},Display : function() {document.getElementsByClassName('container')[0].style.visibility = 'visible';},Get_page : function(page) {const xhr = new XMLHttpRequest();xhr.open(\"GET\", \"https://tle-server.herokuapp.com/get/tle?satname=\" + pagePagination.satname + \"&page=\" + page);xhr.responseType = 'json';xhr.onload = () => {if (xhr.status == 200) {if (xhr.response[\"empty_result\"] == false) {var table_html = '<thead><tr><th>Satellite name</th><th>Norad ID</th> </tr></thead>';var jsonResponse = xhr.response[\"list_of_variants\"];for (var num = 0; num < jsonResponse.length; num++) {table_html += '<tr class=\"tableOption\" id=\"' + jsonResponse[num][\"norad_id\"] + '\"> <th>' + jsonResponse[num][\"object_name\"] + \"</th>\";table_html +=\"<th>\" + jsonResponse[num][\"norad_id\"] + \"</th> </tr>\";}if (jsonResponse.length < 10 && page != 1) {for (var num = jsonResponse.length; num < 10; num++){table_html += '<tr class=\"disabledTableOption\"><th>&nbsp;</th><th>&nbsp;</th></tr>';}}document.getElementById('tableOfValid').innerHTML = table_html;pagePagination.BindOptions();}}};xhr.send(null);}};var init = function(satname) {pagePagination.Init(document.getElementById('paginationID'), {size : 30,page : 1,step : 1,},satname);};var hideTable = function() {pagePagination.Hide();};var displayTable = function() {pagePagination.Display();};</script><style type=\"text/css\">.container {display: none;vertical-align: middle;padding: 1px 2px 4px 2px;background-color: #161616;}#paginationID {flex-basis: 100%;vertical-align: middle;padding-top: 0.5em;}#tableOfValid {border-color: #161616;font-size: 0.75em;flex-basis: 100%;width: 80%;display: table;vertical-align: middle;padding: 1px 1px 1px 1px;background-color: #000000;}#tableOfValid tbody .tableOption:hover {background-color: #222222;}#tableOfValid tbody tr:active {background-color: #0A0A0A;}#tableOfValid tr {margin: 0 2px 0 2px;min-height: 1em;background-color: #161616;}#tableOfValid thead tr{background-color: #101010;}#tableOfValid tbody .tableOption {cursor: pointer;}#paginationID a, #paginationID i {display: inline-block;vertical-align: middle;width: 22px;text-align: center;font-size: 10px;padding: 3px 5px 2px 5px;-webkit-user-select: none;-moz-user-select: none;-ms-user-select: none;-o-user-select: none;user-select: none;}#paginationID a {margin: 0 1px 0 1px;cursor: pointer;color: #e6e6e6;font-size: 0.75em;background-color: #101010;}#paginationID a.current {border: 1px solid #E9E9E9;box-shadow: 0 1px 1px #999;background-color: #101010;}</style><style>.load_page {display: none;position: fixed;height: 100%;width:100%;top: 0;bottom: 0;left: 0;right: 0;background-color: rgba(255,255,255,0.75);}.loading {position: fixed;z-index: 999;height: 2em;width: 2em;overflow: visible;margin: auto;top: 0;left: 0;bottom: 0;right: 0;}/* Transparent Overlay */.loading:before {content: '';display: block;position: fixed;top: 0;left: 0;width: 100%;height: 100%;background-color: rgba(0,0,0,0.3);}/* :not(:required) hides these rules from IE9 and below */.loading:not(:required) {/* hide \"loading...\" text */font: 0/0 a;color: transparent;text-shadow: none;background-color: transparent;border: 0;}.loading:not(:required):after {content: '';display: block;font-size: 10px;width: 1em;height: 1em;margin-top: -0.5em;-webkit-animation: spinner 1500ms infinite linear;-moz-animation: spinner 1500ms infinite linear;-ms-animation: spinner 1500ms infinite linear;-o-animation: spinner 1500ms infinite linear;animation: spinner 1500ms infinite linear;border-radius: 0.5em;-webkit-box-shadow: rgba(0, 0, 0, 0.75) 1.5em 0 0 0, rgba(0, 0, 0, 0.75) 1.1em 1.1em 0 0, rgba(0, 0, 0, 0.75) 0 1.5em 0 0, rgba(0, 0, 0, 0.75) -1.1em 1.1em 0 0, rgba(0, 0, 0, 0.5) -1.5em 0 0 0, rgba(0, 0, 0, 0.5) -1.1em -1.1em 0 0, rgba(0, 0, 0, 0.75) 0 -1.5em 0 0, rgba(0, 0, 0, 0.75) 1.1em -1.1em 0 0;box-shadow: rgba(0, 0, 0, 0.75) 1.5em 0 0 0, rgba(0, 0, 0, 0.75) 1.1em 1.1em 0 0, rgba(0, 0, 0, 0.75) 0 1.5em 0 0, rgba(0, 0, 0, 0.75) -1.1em 1.1em 0 0, rgba(0, 0, 0, 0.75) -1.5em 0 0 0, rgba(0, 0, 0, 0.75) -1.1em -1.1em 0 0, rgba(0, 0, 0, 0.75) 0 -1.5em 0 0, rgba(0, 0, 0, 0.75) 1.1em -1.1em 0 0;}/* Animation */@-webkit-keyframes spinner {0% {-webkit-transform: rotate(0deg);-moz-transform: rotate(0deg);-ms-transform: rotate(0deg);-o-transform: rotate(0deg);transform: rotate(0deg);}100% {-webkit-transform: rotate(360deg);-moz-transform: rotate(360deg);-ms-transform: rotate(360deg);-o-transform: rotate(360deg);transform: rotate(360deg);}}@-moz-keyframes spinner {0% {-webkit-transform: rotate(0deg);-moz-transform: rotate(0deg);-ms-transform: rotate(0deg);-o-transform: rotate(0deg);transform: rotate(0deg);}100% {-webkit-transform: rotate(360deg);-moz-transform: rotate(360deg);-ms-transform: rotate(360deg);-o-transform: rotate(360deg);transform: rotate(360deg);}}@-o-keyframes spinner {0% {-webkit-transform: rotate(0deg);-moz-transform: rotate(0deg);-ms-transform: rotate(0deg);-o-transform: rotate(0deg);transform: rotate(0deg);}100% {-webkit-transform: rotate(360deg);-moz-transform: rotate(360deg);-ms-transform: rotate(360deg);-o-transform: rotate(360deg);transform: rotate(360deg);}}@keyframes spinner {0% {-webkit-transform: rotate(0deg);-moz-transform: rotate(0deg);-ms-transform: rotate(0deg);-o-transform: rotate(0deg);transform: rotate(0deg);}100% {-webkit-transform: rotate(360deg);-moz-transform: rotate(360deg);-ms-transform: rotate(360deg);-o-transform: rotate(360deg);transform: rotate(360deg);}}</style><body><div class=\"load_page\"><div class=\"loading\">Loading&#8230;</div></div><div class=\"current_info\"><div class=\"satellite_info\">Текущий спутник:<ol><li id=\"satellite_name\">Имя:</li><li id=\"satellite_coords\">Координаты:</li><li id=\"satellite_distance\">Дистанция:</li><li id=\"satellite_epoch\">Информация о времени замера TLE:</li><li id=\"satellite_error\">Информация об ошибке в замере из-<br>за расхождения времени:</li><li id=\"satellite_measurement_time\">Время рассчёта положения спутника:</li><li id=\"satellite_vision_status\">Статус возможности наблюдения:</li></ol></div><div class=\"station_info\">Информация о станции:<ol><li id=\"station_time\">Текущее время: <br></li><li id=\"station_coords\">Координаты станции: <br></li><li id=\"station_angle_fix\">Ручная поправка углов наведения:<br></li></ol></div></div><button id=\"reload_data\">Обновить данные</button><div class=\"editors\" style=\"display: inline-block;\"><div class=\"manual_angle_fix\"><div>Ввести ручную поправку углов наведения:</div><table class=\"manual_tle\"><tr><td class=\"key\">Горизонтальная, °</td><td class=\"value\"><input type=\"text\" id=\"azimuth\"></td></tr><tr><td class=\"key\">Вертикальная, °</td><td class=\"value\"><input type=\"text\" id=\"elevation\"></td><td class=\"submit\"><div class=\"image\" id=\"div_angle_fix_submit\"></div></td></tr></table></div><div class=\"auto_tle\"><div>Получить информацию о спутнике с сервера:<br><br></div><div style=\"display:table\"><table class=\"optional_auto_tle\" style=\"display:table-row\"><thead><tr><th colspan=\"3\">ИЛИ<br></th></tr></thead><tbody><tr><td style=\"\">Введите имя спутника или его часть</td><td style=\"padding-left: 2%\">Введите Norad ID спутника</td></tr><tr><td style=\"text-align: left;\"><input type=\"text\" id=\"satname\" style=\"width:100%\"></td><td style=\"text-align: right; padding-left: 2%\"><input type=\"text\" id=\"norad_id\" style=\"width:100%\"></td></tr></tbody></table><div style=\"display:block; width:90%\"><div class=\"container\" style=\"visibility: collapse; display:table-cell\"><table id=\"tableOfValid\"></table><div id=\"paginationID\"></div></div><div class=\"image\" id=\"div_auto_tle_submit\" style=\"display:table-cell; vertical-align: top\"></div></div></div></div><div class=\"detailed_tle\"><button class=\"dropout\">Ввести TLE вручную:</button><table class=\"manual_tle\" style=\"display: none\"><tr><td class=\"key\">TLE1</td><td class=\"value\"><input type=\"text\" id=\"tle1\"></td></tr><tr><td class=\"key\">TLE2</td><td class=\"value\"><input type=\"text\" id=\"tle2\"></td></tr><tr><td class=\"key\">Имя<br>(необязательно)</td><td class=\"value\"><input type=\"text\" id=\"name\"></td><td class=\"submit\"><div class=\"image\" id=\"div_manual_tle_submit\"></div></td></tr></table></div><div class=\"current_time\"><div>Ввести текущее время:</div><table class=\"manual_tle\"><tr><td class=\"key\">Вручную (unixtime)</td><td class=\"value\"><input type=\"text\" id=\"unixtime\"></td><td class=\"submit\"><div class=\"image\" id=\"div_cur_time_submit\"></div></td></tr><tr><td class=\"key\">С сервера</td><td class=\"value\"><button id=\"auto_unixtime\" style=\"width: 98%; text-align: center;\">Запросить текущее время</button></td></tr></table></div><div class=\"coordinates\"><div>Ввести координаты станции:</div><table class=\"manual_tle\"><tr><td class=\"key\">Широта, °</td><td class=\"value\"><input type=\"text\" id=\"latitude\"></td></tr><tr><td class=\"key\">Долгота, °</td><td class=\"value\"><input type=\"text\" id=\"longitude\"></td></tr><tr><td class=\"key\">Высота, м</td><td class=\"value\"><input type=\"text\" id=\"altitude\"></td><td class=\"submit\"><div class=\"image\" id=\"div_coords_submit\"></div></td></tr></table></div></div><div id=\"exception_window\"><div id=\"exc_code\"></div><div id=\"exc_message\"></div></div></body><script type=\"text/javascript\">var drop_block = document.getElementsByClassName(\"dropout\");var i;for (i = 0; i < drop_block.length; i++) {drop_block[i].addEventListener(\"click\", function() {this.classList.toggle(\"active\");var panel = this.nextElementSibling;if (panel.style.display === \"inline-block\") {panel.style.display = \"none\";} else {panel.style.display = \"inline-block\";}});};ArduinoTimeOut = 15000;function sleep(ms) {return new Promise(resolve => setTimeout(resolve, ms));};function raiseException(status_code, message) {hide_loader();block = document.getElementById('exception_window'); status_block = document.getElementById('exc_code');message_block = document.getElementById('exc_message');status_block.innerHTML = status_code;message_block.innerHTML = message;block.style.display = 'block';sleep(5000).then(() => block.style.display = 'none');};function cleanUp() {fields = document.getElementsByTagName('input');for (var i = 0; i < fields.length; ++i){fields[i].value = \"\";}};function sendToArduino(method_name, data){const xhr = new XMLHttpRequest();var AurduinoURL = './update';var request_data = {};request_data['method'] = method_name;request_data['data'] = data;xhr.open('POST', AurduinoURL);xhr.responseType = 'json';xhr.setRequestHeader(\"Content-Type\", \"application/json;charset=UTF-8\");xhr.timeout = ArduinoTimeOut;xhr.onload = () => {if (xhr.status != 200) {raiseException(xhr.status, xhr.response['error']);return null;}};xhr.onerror = () => {raiseException('Where is Arduino?', 'There is no connetion to Arduino');};xhr.ontimeout = () => {raiseException('Turtle time!', 'Time limit with Arduino exceeded');};xhr.onloadend = () => {hide_loader();if (xhr.status == 200){fillPattern(xhr.response);window.scrollTo({ top: 0, behavior: 'smooth' });cleanUp();}};xhr.send(JSON.stringify(request_data));};function hide_loader() {document.getElementsByClassName('load_page')[0].style.display = 'none';buttons = document.getElementsByClassName('image');for (var i = 0; i < buttons.length; ++i){buttons[i].style.visibility = 'visible';};};function display_loader() {document.getElementsByClassName('load_page')[0].style.display = 'block';buttons = document.getElementsByClassName('image');for (var i = 0; i < buttons.length; ++i) {buttons[i].style.visibility = 'collapse';};sleep(30000).then(() => hide_loader());};function autoTLESubmit() {display_loader();const xhr = new XMLHttpRequest();var satname = document.getElementById('satname').value;var norad_id = document.getElementById('norad_id').value;if (norad_id !== \"\") {xhr.open(\"GET\", \"https://tle-server.herokuapp.com/get/tle?norad_id=\" + norad_id);}else if(satname !== \"\") {xhr.open(\"GET\", \"https://tle-server.herokuapp.com/get/tle?satname=\" + satname);}else {raiseException('0_o', 'Input any of two fields please (=)');return null;}xhr.responseType = 'json';xhr.timeout = ArduinoTimeOut;xhr.onerror = () => {raiseException('TeaPod', 'There is no internet connection \\o-o/');};xhr.ontimeout = () => {raiseException('Bad connection', 'Time limit exceeded');};xhr.onload = () => {if (xhr.status != 200) {raiseException(xhr.status, 'Something wents horrible with Heroku \\o-o/');return null;}if (xhr.response['empty_result']){raiseException('416 - TeaPod', xhr.response['reason']);return null;}if (xhr.response['is_only_one_result'] === false){init(satname);displayTable();return null;}hideTable();var data = xhr.response['satellite_identity'];sendToArduino('tle', data);};xhr.onloadend = () => {hide_loader();};xhr.send();};function manualTLESubmit() {display_loader();tle1 = document.getElementById('tle1').value;tle2 = document.getElementById('tle2').value;if (tle1 === '' || tle2 === ''){raiseException('Not enough data', 'Please input required fields (tle-1 AND tle-2)');return null;}var data = {'tle1': tle1, 'tle2': tle2};name = document.getElementById('name').value;if (name !== \"\"){data['name'] = name;}sendToArduino('tle', data);};function currentTimeSubmit() {display_loader();unixtime = document.getElementById('unixtime').value;var re = RegExp('^[0-9]{1,15}$');if (!unixtime.match(re)) {raiseException('Bad data format', 'Unixtime must contain only digits');return null;}var data ={'unixtime': unixtime, 'from_server': false};sendToArduino('current_time', data);};function autoTimeSubmit() {display_loader();var data = {'from_server': true};sendToArduino('current_time', data);};function coordsSubmit() {display_loader();var re = RegExp('^[0-9]*\\\.{0,1}[0-9]*$');longitude = document.getElementById('longitude').value;latitude = document.getElementById('latitude').value;altitude = document.getElementById('altitude').value;var data = {};if (longitude !== '' && longitude.match(re)){data['longitude'] = longitude;}if (latitude !== '' && latitude.match(re)){data['latitude'] = latitude;}if (altitude !== '' && altitude.match(re)){data['altitude'] = altitude;}if (Object.keys(data).length === 0) {raiseException('Bad coords input','Write at least 1 coordinate using only digits and dot for double');return null;}sendToArduino('coords', data);};function angleFixSubmit() {display_loader();var re = RegExp('^[0-9]*\\\.{0,1}[0-9]*$');azimuth = document.getElementById('azimuth').value;elevation = document.getElementById('elevation').value;var data = {};if (azimuth !== '' && azimuth.match(re)){data['azimuth'] = azimuth;}if (elevation !== '' && elevation.match(re)){data['elevation'] = elevation;}if (Object.keys(data).length === 0) {raiseException('Bad coords input','Write at least 1 angle using only digits and dot for double');return null;}sendToArduino('angle_fix', data);}function makeListenerOnButtons() {var auto_time_submit = document.getElementById('reload_data');auto_time_submit.addEventListener('click', requestPatternData);var auto_tle_submit = document.getElementById('auto_tle_submit');auto_tle_submit.addEventListener('click', autoTLESubmit);var manual_tle_submit = document.getElementById('manual_tle_submit');manual_tle_submit.addEventListener('click', manualTLESubmit);var cur_time_submit = document.getElementById('cur_time_submit');cur_time_submit.addEventListener('click', currentTimeSubmit);var auto_time_submit = document.getElementById('auto_unixtime');auto_time_submit.addEventListener('click', autoTimeSubmit);var coords_submit = document.getElementById('coords_submit');coords_submit.addEventListener('click', coordsSubmit);var angle_fix_submit = document.getElementById('angle_fix_submit');angle_fix_submit.addEventListener('click', angleFixSubmit);};document.addEventListener('DOMContentLoaded', makeListenerOnButtons, false);var datetime = 0;var timerVariable = setInterval(countUpTimer, 1000);function countUpTimer() {if (datetime instanceof Date){datetime = new Date(datetime.getTime() + 1000);station_time = document.getElementById('station_time');station_time.innerHTML = 'Текущее время:<br>' + datetime.toLocaleString();}};function fillPattern(data){satellite_name = document.getElementById('satellite_name');satellite_name.innerHTML = 'Имя:<br>' + data['satellite_name'];satellite_coords = document.getElementById('satellite_coords');satellite_coords.innerHTML = 'Координаты:<br>' + data['satellite_coords'];satellite_distance = document.getElementById('satellite_distance');satellite_distance.innerHTML = 'Дистанция:<br>' + data['satellite_distance'];satellite_epoch = document.getElementById('satellite_epoch');if (parseInt(data['satellite_epoch'])){satellite_epoch_date = new Date(1000 * parseInt(data['satellite_epoch']));satellite_epoch.innerHTML = 'Информация о времени замера TLE:<br>' + satellite_epoch_date.toLocaleString();}else{satellite_epoch.innerHTML = 'Информация о времени замера TLE:<br>' + data['satellite_epoch'];}satellite_error = document.getElementById('satellite_error');satellite_error.innerHTML = 'Информация об ошибке в замере из-<br>за расхождения времени:<br>' + data['satellite_error'];if (data['is_satellite_allowed']) {satellite_measurement_time = document.getElementById('satellite_measurement_time');sat_mes_date = new Date(Date.parse(data['satellite_measurement_time']));satellite_measurement_time.innerHTML = 'Время рассчёта положения спутника:<br>' + sat_mes_date.toLocaleString();}else {satellite_measurement_time.innerHTML = 'Время рассчёта положения спутника:<br>' + data['satellite_measurement_time'];}satellite_vision_status = document.getElementById('satellite_vision_status');satellite_vision_status.innerHTML = 'Статус возможности наблюдения:<br>' + data['satellite_vision_status'];station_coords = document.getElementById('station_coords');station_coords.innerHTML = 'Координаты станции:<br>' + data['station_coords'];station_angle_fix = document.getElementById('station_angle_fix');station_angle_fix.innerHTML = 'Ручная поправка углов наведения:<br>' + data['station_angle_fix'];datetime = new Date(1000 * parseInt(data['station_time']));datetime = new Date(datetime.getTime());};function requestPatternData() {ArduinoDataSource = './info';const xhr = new XMLHttpRequest();xhr.open('GET', ArduinoDataSource);xhr.responseType = 'json';xhr.onload = () => {if (xhr.status != 200){raiseException('=( Грустный тромбон )=', 'Не могу подгрузить текущие данные с Arduino. Обратитесь к программисту плезб');return null;}fillPattern(xhr.response);};xhr.onloadend = () => {hide_loader();};xhr.send();};document.addEventListener('DOMContentLoaded', requestPatternData, false);setInterval(requestPatternData, 30000);angle_fix_submit = document.getElementsByClassName('manual_angle_fix')[0];angle_fix_submit.addEventListener('keypress', function (e) {if (e.key === 'Enter') {angleFixSubmit();}});auto_tle_submit = document.getElementsByClassName('auto_tle')[0];auto_tle_submit.addEventListener('keypress', function (e) {if (e.key === 'Enter') {autoTLESubmit();}});detailed_tle_submit = document.getElementsByClassName('detailed_tle')[0];detailed_tle_submit.addEventListener('keypress', function (e) {if (e.key === 'Enter') {manualTLESubmit();}});current_time_submit = document.getElementsByClassName('current_time')[0];current_time_submit.addEventListener('keypress', function (e) {if (e.key === 'Enter') {currentTimeSubmit();}});coordinates_submit = document.getElementsByClassName('coordinates')[0];coordinates_submit.addEventListener('keypress', function (e) {if (e.key === 'Enter') {coordsSubmit();}});</script></html>");
}

void notFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); ++i) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  message += server.header("Content-Type");
  server.send(404, "text/plain", message + '\n');
}

} // namespace handlers

String fillZeroes(int number, int n) {
  String result;
  String number_string(number);
  for (int i = 0; i < n - number_string.length(); ++i) {
    result += '0';
  }
  result += number_string;
  return result;
}

String fromUnixtimeToMessage(time_t unixtime) {
  String message;
  message += fillZeroes(hour(unixtime)) + ':' + fillZeroes(minute(unixtime)) + ':' + fillZeroes(second(unixtime)) + ' ';
  message += fillZeroes(day(unixtime)) + '.' + fillZeroes(month(unixtime)) + '.' + String(year(unixtime));
  return message;
}
