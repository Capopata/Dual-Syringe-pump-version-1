#pragma once

#include<stdint.h>

typedef struct{

    /*Controller gains*/
    float Kp;
    float Ki;
    float Kd;
    
    /* Derivative low-pass filter time constant */
	float tau;

    /*Output limits*/
    float limMinOut;
    float limMaxOut;

    /*Integrator limits*/
    float limMinInt;
    float limMaxInt;

    /*Sample time(in seconds)*/
    float T;

    /*Controller "memory"*/
    float integrator;
    float prevError;
    float differentiator;
    float prevMeasurement;

    /*Controller output*/
    float out;

}PIDcontroller;

void PID_Init(PIDcontroller *pid);
float PID_Update(PIDcontroller *pid, float setpoint, float measurement);