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
    PRIMARY KEY( dev_on, dev_off )
);

-- example insertion
-- INSERT INTO device VALUES( 'outlet0', 5592371, 5592380, 189 );

-- ----------------------------------------------------------------------------------------
-- Most useful example queries here
-- ----------------------------------------------------------------------------------------

-- Return list of devices for client
-- SELECT dev_name FROM device;

-- Return dev_on and dev_pulse from device 'outlet0'
-- SELECT dev_on, dev_pulse FROM device WHERE dev_name='outlet0'

-- Return dev_off and dev_pulse from device 'outlet0'
-- SELECT dev_off, dev_pulse FROM device WHERE dev_name='outlet0'

-- ----------------------------------------------------------------------------------------
-- Update values (mostly name) examples
-- ----------------------------------------------------------------------------------------

-- change name 'outlet0' to 'light0'
-- UPDATE device SET dev_name='light0' WHERE dev_name='outlet0';

-- If we had to update the dev_on code, this would be how.
-- It would be a similar process for the dev_off code and dev_pulse.
-- change dev_on from 5592371 to 5592372 on device 'light0'
-- UPDATE device SET dev_on=5592372 WHERE dev_name='light0';