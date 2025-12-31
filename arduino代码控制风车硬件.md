
#include <WiFi.h>
#include <WebSocketMCP.h>

String state;

WebSocketMCP mcpClient;

void onConnectionStatus(bool connected) {
  if (connected) {
    mcpClient.registerTool(
    "windmill",
    "风车",
    R"({"type":"object","properties":{"state":{"type":"string","enum":["on" , "off"]}}  ,"required":["state"]})",
    [](const String& args){
    DynamicJsonDocument doc(256);
    deserializeJson(doc, args);
      if (doc.containsKey("state")) {
        state =  doc["state"].as<String>();
        Serial.println(state);
        if (state == "on") {
          digitalWrite(21, HIGH);
          Serial.println("风车灯开始旋转");

        } else if (state == "off") {
          digitalWrite(21, LOW);
          Serial.println("风车灯停止旋转");
        }

      }
      StaticJsonDocument<200> Responsedoc;
      Responsedoc["success"] = Responsedoc["state"] = state;;

      String ResponsedocString;
      serializeJson(Responsedoc, ResponsedocString);
      return WebSocketMCP::ToolResponse(ResponsedocString);
    }
    );

  }

}

void setup() {
  Serial.begin(115200);
  WiFi.begin("xrunda-iot", "88888888");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Local IP:");
  Serial.print(WiFi.localIP());

  state = "";
  pinMode(21, OUTPUT);
  while (!(WiFi.status())) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("");
  Serial.println(WiFi.localIP());
  mcpClient.begin("wss://api.xiaozhi.me/mcp/?token=eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VySWQiOjk5MTQsImFnZW50SWQiOjEyNDk3NTUsImVuZHBvaW50SWQiOiJhZ2VudF8xMjQ5NzU1IiwicHVycG9zZSI6Im1jcC1lbmRwb2ludCIsImlhdCI6MTc2Njc0NjgwMywiZXhwIjoxNzk4MzA0NDAzfQ.AvI_Vlr2m-0qZjPo-Aymz8JYd-SyIaBYuKn_NMGF35hHEzln3oNH77H4QSDEUQp-QclkfCLyeYa5j3oM6I-QXA", onConnectionStatus);
}

void loop() {
  mcpClient.loop();

}