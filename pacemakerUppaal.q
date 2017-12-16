//This file was generated from (Academic) UPPAAL 4.1.20-beta9 (rev. 9), June 2017

/*

*/
A[] (Ventricle.set_alarm && Ventricle.Sensed imply Observer.alarmSet==true)

/*

*/
A[] not deadlock

/*

*/
A[] (Ventricle.x < VRP && Ventricle.started) imply (Ventricle.WaitVRP || Ventricle.PostWaitPreSense || Ventricle.Sensed)

/*

*/
A[] ((Ventricle.WaitRI && Ventricle.started && !Ventricle.natural_beat) imply Ventricle.x <= Ventricle.LRI)

/*

*/
A[] ((Ventricle.WaitRI && Ventricle.started && Ventricle.natural_beat) imply Ventricle.x <= Ventricle.HRI)

/*

*/
A[] ((Ventricle.WaitRI && Ventricle.started)) imply Ventricle.x >= VRP\

