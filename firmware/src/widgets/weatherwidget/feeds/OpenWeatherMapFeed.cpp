#include "OpenWeatherMapFeed.h"
#include "../WeatherTranslations.h"
#include "GlobalTime.h"
#include "TaskFactory.h"
#include "config_helper.h"
#include <unordered_map>

OpenWeatherMapFeed::OpenWeatherMapFeed(const String &apiKey, int units)
    : apiKey(apiKey), m_weatherUnits(units) {}

void OpenWeatherMapFeed::setupConfig(ConfigManager &config) {
    // Define the configuration for OpenWeatherMap variables
    config.addConfigString("WeatherWidget", "openWeatherLat", &m_lat_id, 10, t_openWeatherLat);
    config.addConfigString("WeatherWidget", "openWeatherLong", &m_long_id, 10, t_openWeatherLong);
    config.addConfigString("WeatherWidget", "openWeatherName", &m_name, 15, t_openWeatherName);
}

bool OpenWeatherMapFeed::getWeatherData(WeatherDataModel &model) {
    String weatherUnits = m_weatherUnits == 0 ? "metric" : "imperial";
    String lang = I18n::getLanguageString();
    if (lang != "en" && lang != "de" && lang != "fr") {
        lang = "en";
    }

    String httpRequestAddress = String(WEATHER_OPENWEATHERMAP_API_URL) + "?lat=" + m_lat_id.c_str() + "&lon=" + m_long_id.c_str() +
                                +"&appid=" + apiKey + "&units=" + weatherUnits + "&exclude=minutely,hourly,alerts&lang=" + lang +
                                "&cnt=3";

    auto task = TaskFactory::createHttpGetTask(
        httpRequestAddress, [this, &model](int httpCode, const String &response) { processResponse(httpCode, response, model); }, [this](int httpCode, String &response) { preProcessResponse(httpCode, response); });

    if (!task) {
        Log.errorln("Failed to create weather task");
        return false;
    }

    bool success = TaskManager::getInstance()->addTask(std::move(task));
    if (!success) {
        Log.errorln("Failed to add weather task");
    }

    return success;
}
void OpenWeatherMapFeed::preProcessResponse(int httpCode, String &response) {
    if (httpCode > 0) {

        JsonDocument filter;
        filter["current"]["dt"] = true;
        filter["current"]["temp"] = true;
        filter["current"]["weather"][0]["description"] = true;
        filter["current"]["weather"][0]["icon"] = true;

        filter["daily"][0]["dt"] = true;
        filter["daily"][0]["summary"] = true;
        filter["daily"][0]["temp"]["min"] = true;
        filter["daily"][0]["temp"]["max"] = true;
        filter["daily"][0]["weather"][0]["main"] = true;
        filter["daily"][0]["weather"][0]["description"] = true;
        filter["daily"][0]["weather"][0]["icon"] = true;

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response, DeserializationOption::Filter(filter));

        if (!error) {
            response = doc.as<String>();
        } else {
            // Handle JSON deserialization error
            Log.errorln("Deserialization failed: %s", error.c_str());
        }
    }
}

void OpenWeatherMapFeed::processResponse(int httpCode, const String &response, WeatherDataModel &model) {
    if (httpCode > 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response);

        if (!error) {
            // Set city name
            model.setCityName(m_name.c_str());

            model.setCurrentTemperature(doc["current"]["temp"].as<float>());
            model.setCurrentText(doc["daily"][0]["summary"].as<String>());
            model.setCurrentIcon(translateIcon(doc["current"]["weather"][0]["icon"]));

            model.setTodayHigh(doc["daily"][0]["temp"]["max"].as<float>());
            model.setTodayLow(doc["daily"][0]["temp"]["min"].as<float>());

            Log.traceln("Weather data: %s", response.c_str());
            for (int i = 1; i < 4; i++) {
                model.setDayIcon(i - 1, translateIcon(doc["daily"][i]["weather"]["icon"]));
                model.setDayHigh(i - 1, doc["daily"][i]["temp"]["max"].as<float>());
                model.setDayLow(i - 1, doc["daily"][i]["temp"]["min"].as<float>());
            }

        } else {
            // Handle JSON deserialization error
            Log.errorln("Deserialization failed: %s", error.c_str());
        }
    } else {
        Log.errorln("HTTP request failed, error code: %d\n", httpCode);
    }
}
String OpenWeatherMapFeed::translateIcon(const std::string &icon) {
    // Define the mapping of input strings to simplified weather icons
    static const std::unordered_map<std::string, std::string> iconMapping = {
        {"01d", "clear-day"},
        {"01n", "clear-night"},
        {"02d", "partly-cloudy-day"},
        {"02n", "partly-cloudy-night"},
        {"03d", "partly-cloudy-day"},
        {"03n", "partly-cloudy-night"},
        {"04d", "partly-cloudy-day"},
        {"04n", "partly-cloudy-night"},
        {"09d", "rain"},
        {"09n", "rain"},
        {"10d", "rain"},
        {"10n", "rain"},
        {"11d", "rain"},
        {"11n", "rain"},
        {"13d", "snow"},
        {"13n", "snow"},
        {"50d", "fog"},
        {"50n", "fog"}};

    // Find the icon in the mapping and return the corresponding simplified value
    auto it = iconMapping.find(icon);
    if (it != iconMapping.end()) {
        return String(it->second.c_str());
    }
    return "clear-day";
}
