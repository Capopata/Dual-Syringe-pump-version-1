#include<pid.h>

void PID_Init(PIDcontroller *pid){
    /*clear controller variables*/

    pid->integrator = 0.0f;
    pid->prevError = 0.0f;

    pid->differentiator = 0.0f;
    pid->prevMeasurement = 0.0f;

    pid->out = 0.0f;
}

float PID_Update(PIDcontroller *pid, float setpoint, float measurement){
    /*Error*/
    float error = setpoint - measurement;

    /*Proportional*/
    float proportional = pid->Kp *error;

    /*Integral*/
    pid->integrator = pid->integrator + 0.5f * pid->Ki * pid->T * (error + pid->prevError);

    /*Anti-windup via integrator clamping*/
    if(pid->integrator > pid->limMaxInt){
        pid->integrator = pid->limMaxInt;
    }else if(pid->integrator <pid->limMinInt){
        pid->integrator = pid->limMinInt;
    }

    /*Derivative*/
    pid->differentiator = -(2.0 * pid->Kd *(measurement - pid->prevMeasurement))
                        + (2.0f * pid->tau - pid->T) * pid->differentiator
                        /(2.0 * pid->tau + pid->T);
    
    /*Calculate output*/
    pid->out = proportional + pid->integrator + pid->differentiator;

    if(pid->out > pid->limMaxOut){
        pid->out = pid->limMaxOut;
    }else if(pid->out < pid->limMinOut){
        pid->out = pid->limMinOut;
    }
    
    /* Store error and measurement for later use */
    pid->prevError       = error;
    pid->prevMeasurement = measurement;

	/* Return controller output */
    return pid->out;
}