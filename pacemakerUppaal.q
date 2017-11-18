//This file was generated from (Commercial) UPPAAL 4.0.14 (rev. 5615), May 2014

/*

*/
A[] not deadlock

/*

*/
A[] (Ventricle.WaitVRP && Heart.Idle) imply x <= maxVRP

/*

*/
A[] ((Ventricle.WaitRI && Ventricle.started && !Ventricle.natural_beat) imply x <= Ventricle.LRI)

/*

*/
A[] ((Ventricle.WaitRI && Ventricle.started && Ventricle.natural_beat) imply x <= Ventricle.URI)

/*

*/
A[] ((Ventricle.WaitRI && Ventricle.started) imply x >= minVRP\

