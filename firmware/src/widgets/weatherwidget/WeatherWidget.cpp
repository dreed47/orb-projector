// TODO:
// 1
// factor out a selectDisplay that selects the dispay and returns the workable object,
// so we don't have this select, getDisplay crap all over the place
// 2
// make high/low an enum, if we even keep it (I strongly suggest just switching back
// and forth between high and low every 10 seconds or something)
// 3
// factor out the text wrapping (there's a utils for that already, if that doesn't work, why not?)

#include "WeatherWidget.h"
#include "WeatherTranslations.h"
#include "feeds/OpenWeatherMapFeed.h"
#include "feeds/TempestFeed.h"
#include "feeds/VisualCrossingFeed.h"
#include "icons.h"
#include <ArduinoJson.h>
#include <ArduinoLog.h>

WeatherWidget::WeatherWidget(ScreenManager &manager, ConfigManager &config)
    : Widget(manager, config),
      m_drawTimer(addDrawRefreshFrequency(WEATHER_DRAW_DELAY)),
      m_updateTimer(addUpdateRefreshFrequency(WEATHER_UPDATE_DELAY)) {
    m_enabled = (INCLUDE_WEATHER == WIDGET_ON);
    m_config.addConfigBool("WeatherWidget", "weatherEnabled", &m_enabled, t_enableWidget);
    weatherFeed = createWeatherFeed();
    weatherFeed->setupConfig(config); // allow feed to add its own config
    m_config.addConfigComboBox("WeatherWidget", "weatherUnits", &m_weatherUnits, t_temperatureUnits, t_temperatureUnit, true);
    m_config.addConfigComboBox("WeatherWidget", "weatherScrMode", &m_screenMode, t_screenModes, t_screenMode, true);
    m_config.addConfigInt("WeatherWidget", "weatherCycleHL", &m_switchinterval, t_weatherCycleHL, true);
    Log.noticeln("WeatherWidget initialized, mode=%d", m_screenMode);
    m_mode = MODE_HIGHS;
}

WeatherWidget::~WeatherWidget() {
    delete weatherFeed;
}

WeatherFeed *WeatherWidget::createWeatherFeed() {

    int weatherUnits = m_config.getConfigInt("weatherUnits", m_weatherUnits);

#if WEATHER_OPENWEATHERMAP_FEED
    return new OpenWeatherMapFeed(WEATHER_OPENWEATHERMAP_API_KEY, weatherUnits);
#elif WEATHER_TEMPEST_FEED
    return new TempestFeed(WEATHER_TEMPEST_API_KEY, weatherUnits);
#elif WEATHER_VISUALCROSSING_FEED
    return new VisualCrossingFeed(WEATHER_VISUALCROSSING_API_KEY, weatherUnits);
#endif
}

void WeatherWidget::changeMode() {
    m_prevMillisSwitch = millis();
    m_mode = (m_mode == MODE_HIGHS) ? MODE_LOWS : MODE_HIGHS;
    threeDayWeather(4);
}

void WeatherWidget::buttonPressed(uint8_t buttonId, ButtonState state) {
    if (buttonId == BUTTON_OK && state == BTN_SHORT)
        changeMode();
    if (buttonId == BUTTON_OK && state == BTN_MEDIUM)
        update(true);
}

void WeatherWidget::setup() {
    m_time = GlobalTime::getInstance();
    configureColors();
    m_prevMillisSwitch = millis();
}

void WeatherWidget::draw(bool force) {
    m_manager.setFont(DEFAULT_FONT);
    m_time->updateTime();
    int clockStamp = getClockStamp();
    if (clockStamp != m_clockStamp || force) {
        displayClock(0);
        m_clockStamp = clockStamp;
    }

    if (force || model.isChanged()) {
        weatherText(1);
        drawWeatherIcon(2, model.getCurrentIcon(), 0, 0, 1);
        singleWeatherDeg(3);
        threeDayWeather(4);
        model.setChangedStatus(false);
        if (force) {
            resetTimer(m_drawTimer); // Reset only on forced draw
        }
    }

    if ((millis() - m_prevMillisSwitch >= (m_switchinterval * 1000)) && m_switchinterval > 0) {
        changeMode();
        m_prevMillisSwitch = millis(); // Reset timer
    }
}

void WeatherWidget::update(bool force) {
    if (force) {
        int retry = 0;
        while (!weatherFeed->getWeatherData(model) && retry++ < MAX_RETRIES)
            ;
        resetTimer(m_updateTimer); // Reset timer after forced update
    } else {
        weatherFeed->getWeatherData(model);
    }
}

void WeatherWidget::displayClock(int displayIndex) {
    const int clockY = 120;
    const int dayOfWeekY = 190;
    const int dateY = 50;

    m_manager.selectScreen(displayIndex);
    m_manager.fillScreen(m_backgroundColor);
    m_manager.setFontColor(m_foregroundColor);

    m_manager.drawCentreString(m_time->getDayAndMonth(), centre, dateY, 18);
    const String weekDay = m_time->getWeekday();
    m_manager.drawCentreString(weekDay, centre, dayOfWeekY, 22);

    m_manager.drawString(m_time->getHourPadded(), centre - 10, clockY, 66, Align::MiddleRight);
    m_manager.drawString(":", centre, clockY, 66, Align::MiddleCenter);
    m_manager.drawString(m_time->getMinutePadded(), centre + 10, clockY, 66, Align::MiddleLeft);
}

// Write an image to the screen from a hex array.
// scale of the image (1=full size, then multiples of 2 to scale down)
// getting the byte array size is very annoying as it's computed on compile, so you can't do it dynamically.
void WeatherWidget::showJPG(int displayIndex, int x, int y, const byte jpgData[], int jpgDataSize, int scale) {
    m_manager.selectScreen(displayIndex);

    TJpgDec.setJpgScale(scale);
    uint16_t w = 0, h = 0;
    TJpgDec.getJpgSize(&w, &h, jpgData, jpgDataSize);
    TJpgDec.drawJpg(x, y, jpgData, jpgDataSize);
}

// Take the text output from the weather API and map it to a icon/byte array, then display it
void WeatherWidget::drawWeatherIcon(int displayIndex, const String &condition, int x, int y, int scale) {
    const byte *iconStart = NULL;
    const byte *iconEnd = NULL;

    if (condition == "partly-cloudy-night") {
        iconStart = m_screenMode == Light ? moonCloudW_start : moonCloudB_start;
        iconEnd = m_screenMode == Light ? moonCloudW_end : moonCloudB_end;
    } else if (condition == "partly-cloudy-day") {
        iconStart = m_screenMode == Light ? sunCloudsW_start : sunCloudsB_start;
        iconEnd = m_screenMode == Light ? sunCloudsW_end : sunCloudsB_end;
    } else if (condition == "clear-day") {
        iconStart = m_screenMode == Light ? sunW_start : sunB_start;
        iconEnd = m_screenMode == Light ? sunW_end : sunB_end;
    } else if (condition == "clear-night") {
        iconStart = m_screenMode == Light ? moonW_start : moonB_start;
        iconEnd = m_screenMode == Light ? moonW_end : moonB_end;
    } else if (condition == "snow") {
        iconStart = m_screenMode == Light ? snowW_start : snowB_start;
        iconEnd = m_screenMode == Light ? snowW_end : snowB_end;
    } else if (condition == "rain") {
        iconStart = m_screenMode == Light ? rainW_start : rainB_start;
        iconEnd = m_screenMode == Light ? rainW_end : rainB_end;
    } else if (condition == "fog" || condition == "wind" || condition == "cloudy") {
        iconStart = m_screenMode == Light ? cloudsW_start : cloudsB_start;
        iconEnd = m_screenMode == Light ? cloudsW_end : cloudsB_end;
    } else {
        Log.warningln("Unknown weather icon: %s", condition.c_str());
    }

    const int size = iconEnd - iconStart;
    if (iconStart != NULL && size > 0) {
        showJPG(displayIndex, x, y, iconStart, size, scale);
    }
}

// Displays the current temperature on a single screen.
// doesn't round deg, just removes all text after the decimal
void WeatherWidget::singleWeatherDeg(int displayIndex) {
    m_manager.selectScreen(displayIndex);
    m_manager.fillScreen(m_backgroundColor);
    m_manager.drawCentreString(model.getCurrentTemperature(0), centre, 90, 88);

    // No glaring white chunks in Dark mode
    if (m_screenMode == Light) {
        m_manager.fillRect(0, 150, 240, 90, m_foregroundColor);
        m_manager.fillRect(centre - 1, 150, 2, 90, m_backgroundColor);
    }

    int fontSize = 22;
    m_manager.setFontColor(m_invertedForegroundColor);
    m_manager.setBackgroundColor(m_invertedBackgroundColor);
    m_manager.drawCentreString("High", 80, 170, fontSize);
    m_manager.drawCentreString("Low", 160, 170, fontSize);
    m_manager.drawCentreString(model.getTodayHigh(0), 80, 210, fontSize);
    m_manager.drawCentreString(model.getTodayLow(0), 160, 210, fontSize);
    m_manager.setFontColor(m_foregroundColor);
    m_manager.setBackgroundColor(m_backgroundColor);
}

// Display the user's current city and the text description of the weather
void WeatherWidget::weatherText(int displayIndex) {
    m_manager.selectScreen(displayIndex);

    //=== TEXT OVERFLOW ============================
    // This takes a given string a and breaks it down in max x character long strings ensuring not to break it only at a space.
    // Given the small width of the screens this will porbablly be needed to this project again so making sure to outline it
    // clearly as this should liekly eventually be turned into a fucntion. Before use the array size should be made to be dynamic.
    // In this case its used for the weather text description

    String message = model.getCurrentText() + " ";
    String messageArr[4];
    int variableRangeS = 0;
    int variableRangeE = 24;
    for (int i = 0; i < 4; i++) {
        while (message.substring(variableRangeE - 1, variableRangeE) != " ") {
            variableRangeE--;
        }
        messageArr[i] = message.substring(variableRangeS, variableRangeE);
        variableRangeS = variableRangeE;
        variableRangeE = variableRangeS + 24;
    }
    //=== OVERFLOW END ==============================

    m_manager.fillScreen(m_backgroundColor);
    String cityName = model.getCityName();
    cityName.remove(cityName.indexOf(",", 0));

    m_manager.setFontColor(m_foregroundColor);
    m_manager.drawFittedString(cityName, centre, 70, 195, 50, Align::MiddleCenter);

    auto y = 118;
    for (auto i = 0; i < 4; i++) {
        m_manager.drawCentreString(messageArr[i], centre, y, 15);
        y += 25;
    }
}

// Displays the next 3 days' weather forecast
void WeatherWidget::threeDayWeather(int displayIndex) {
    const int days = 3;
    const int columnSize = 75;
    const int highLowY = 210;

    m_manager.selectScreen(displayIndex);
    m_manager.fillScreen(m_backgroundColor);
    int fontSize = 22;

    // No glaring white chunks in Dark mode
    if (m_screenMode == Light) {
        m_manager.fillRect(0, 180, 240, 70, m_foregroundColor);
    }

    m_manager.drawString(m_mode == MODE_HIGHS ? "Highs" : "Lows", centre, highLowY, fontSize, Align::MiddleCenter, m_invertedForegroundColor, m_invertedBackgroundColor);
    // Reset colors
    m_manager.setFontColor(m_foregroundColor);
    m_manager.setBackgroundColor(m_backgroundColor);

    int temperatureFontSize = fontSize; // 0-9 only
    // Look up all the temperatures, and if any of them are more than 2 digits, we need
    // to scale down the font -- or it won't look right on the screen.
    String temps[days];
    for (auto i = 0; i < days; i++) {
        temps[i] = m_mode == MODE_HIGHS ? model.getDayHigh(i, 0) : model.getDayLow(i, 0);
        if (temps[i].length() > 4) {
            // We've got a nutty 3-digit temperature (plus degree sign), scale down
            temperatureFontSize = fontSize - 4; // smaller
        }
    }

    m_manager.setFontColor(m_foregroundColor);
    for (auto i = 0; i < days; i++) {
        // TODO: only works for 3 days
        const int x = (centre - columnSize) + i * columnSize;

        drawWeatherIcon(displayIndex, model.getDayIcon(i), x - 30, 40, 4);
        m_manager.drawCentreString(temps[i], x, 122, temperatureFontSize);

        String shortDayName = i18n(t_weekdays, weekday(m_time->getUnixEpoch() + (86400 * (i + 1))) - 1);
        shortDayName.remove(3);
        m_manager.drawString(shortDayName, x, 154, fontSize, Align::MiddleCenter);
    }
}

int WeatherWidget::getClockStamp() {
    return m_time->getHour() * 60 + m_time->getMinute();
}

void WeatherWidget::configureColors() {
    m_foregroundColor = m_screenMode == Light ? TFT_BLACK : TFT_WHITE;
    m_backgroundColor = m_screenMode == Light ? TFT_WHITE : TFT_BLACK;

    // NOTE: In Light mode, we draw decorative black chunks and display the high and low on them in white.
    //       It does not make sense to have glaring white chunks in dark mode, so we don't draw them at all,
    //       and display the high and low in white too.
    m_invertedForegroundColor = m_screenMode == Light ? m_backgroundColor : m_foregroundColor;
    m_invertedBackgroundColor = m_screenMode == Light ? m_foregroundColor : m_backgroundColor;

    m_manager.setBackgroundColor(m_backgroundColor);
}

String WeatherWidget::getName() {
    return "Weather";
}
