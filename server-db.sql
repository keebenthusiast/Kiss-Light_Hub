-- ----------------------------------------------------------------------------------------
-- Simple table used only for toggling the relevant device
-- 
-- It should also be noted that is is primarily designed for use with
-- sqlite3, so use that.
-- 
-- Written by:
--      Christian Kissinger
-- ----------------------------------------------------------------------------------------
CREATE TABLE device (
    dev_name VARCHAR NOT NULL,
    dev_on INT NOT NULL,
    dev_off INT NOT NULL,
    dev_pulse INT NOT NULL,
    dev_toggled INT NOT NULL,
    PRIMARY KEY( dev_name )
);

-- example insertion
-- INSERT INTO device VALUES( 'outlet0', 5592371, 5592380, 189, 0 );

-- ----------------------------------------------------------------------------------------
-- Most useful example queries here
-- ----------------------------------------------------------------------------------------

-- Return list of devices for client
-- SELECT dev_name FROM device;

-- Return dev_on and dev_pulse from device 'outlet0'
-- SELECT dev_on, dev_pulse FROM device WHERE dev_name='outlet0'

-- Return dev_off and dev_pulse from device 'outlet0'
-- SELECT dev_off, dev_pulse FROM device WHERE dev_name='outlet0'

-- Return dev_name from device 'outlet0'
-- SELECT dev_name FROM device WHERE dev_on=5592371 OR dev_off=5592380;

-- ----------------------------------------------------------------------------------------
-- Update values (mostly name) examples
-- ----------------------------------------------------------------------------------------

-- change name 'outlet0' to 'light0'
-- UPDATE device SET dev_name='light0' WHERE dev_name='outlet0';

-- change toggled from 0 to 1 (false to true) on device 'light0'
-- UPDATE device SET dev_toggled=1 WHERE dev_name='light0'

-- If we had to update the dev_on code, this would be how.
-- It would be a similar process for the dev_off code and dev_pulse.
-- change dev_on from 5592371 to 5592372 on device 'light0'
-- UPDATE device SET dev_on=5592372 WHERE dev_name='light0';

-- ----------------------------------------------------------------------------------------
-- Delete Entries example(s)
-- ----------------------------------------------------------------------------------------

-- delete light0 entry
-- DELETE FROM device WHERE dev_name='light0';