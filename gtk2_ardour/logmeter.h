#ifndef __ardour_gtk_log_meter_h__
#define __ardour_gtk_log_meter_h__

#if 0
inline float
_log_meter (float power, double lower_db, double upper_db, double non_linearity)
{
	return (power < lower_db ? 0.0 : pow((power-lower_db)/(upper_db-lower_db), non_linearity));
}

inline float
log_meter (float power)
{
	return _log_meter (power, -192.0, 0.0, 8.0);
}
#endif

inline float
log_meter (float db)
{
         gfloat def = 0.0f; /* Meter deflection %age */
 
         if (db < -70.0f) {
                 def = 0.0f;
         } else if (db < -60.0f) {
                 def = (db + 70.0f) * 0.25f;
         } else if (db < -50.0f) {
                 def = (db + 60.0f) * 0.5f + 2.5f;
         } else if (db < -40.0f) {
                 def = (db + 50.0f) * 0.75f + 7.5f;
         } else if (db < -30.0f) {
                 def = (db + 40.0f) * 1.5f + 15.0f;
         } else if (db < -20.0f) {
                 def = (db + 30.0f) * 2.0f + 30.0f;
         } else if (db < 6.0f) {
                 def = (db + 20.0f) * 2.5f + 50.0f;
         } else {
		 def = 115.0f;
	 }
 
	 /* 115 is the deflection %age that would be 
	    when db=6.0. this is an arbitrary
	    endpoint for our scaling.
	 */

         return def/115.0f;
}

#endif /* __ardour_gtk_log_meter_h__ */
