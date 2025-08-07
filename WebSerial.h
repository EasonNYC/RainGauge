
//Webserial
//AsyncWebServer server(80);
/* Message callback of WebSerial */
/*void recvMsg(uint8_t *data, size_t len){

  WebSerial.println("Received Data...");

  //get the data
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }

  //echo the data received 
  WebSerial.println(d);

  //if print rain conditions
  if(d == "update"){
    rain_gauge.reportRain();
    temp_sensor.reportF();
  } 
 
}*/

//WebSerial is accessible at "<IP Address>/webserial" in browser
  //WebSerial.begin(&server);
  //WebSerial.msgCallback(recvMsg);
  //server.begin();
