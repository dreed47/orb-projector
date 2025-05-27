#ifndef PARQET_DATA_MODEL_H
#define PARQET_DATA_MODEL_H

#include <Arduino.h>
#include <iomanip>

#include "ParqetHoldingDataModel.h"

class ParqetDataModel {
public:
    ParqetDataModel();
    void setHoldings(ParqetHoldingDataModel *holdings, int count);
    ParqetHoldingDataModel &getHolding(int index);
    void setChartData(float *chartData, int count);
    void clearChartData();
    float *getChartData();
    int getHoldingsCount();
    int getChartDataCount();
    void getChartDataScale(uint8_t maxY, float &scale, float &minVal, float &maxVal, float &chartMinVal);

private:
    ParqetHoldingDataModel *m_holdings = nullptr;
    float *m_chartdata = nullptr;
    int m_holdingsCount = 0;
    int m_chartdataCount = 0;
};

#endif // PARQET_DATA_MODEL_H
