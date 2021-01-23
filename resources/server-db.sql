-- ----------------------------------------------------------------------------------------
-- Simple table used only for toggling the relevant device
--
-- It should also be noted that is is primarily designed for use with
-- sqlite3, so use that.
--
-- Written by:
--      Christian Kissinger
-- ----------------------------------------------------------------------------------------

-- ----------------------------------------------------------------------------------------
-- Everything below here is related to the server.
--
-- And this primarily exists for the discovery feature
--
------------------------------------------------------------------------------------------

-- A very simple table, should only hold 1 entry, ideally anyway.
--CREATE TABLE srvr (
--    uuid_str VARCHAR NOT NULL,
--    PRIMARY KEY( uuid_str )
--);

-- Example (and one time) insertion
-- INSERT INTO srvr VALUES( "abcd1234-5432-7890-a34f-6924a3b2f3dd")

-- Return UUID for server if requested
-- SELECT uuid_str FROM srvr

-- ----------------------------------------------------------------------------------------
--  Everything below here is related to the device table
-- ----------------------------------------------------------------------------------------
CREATE TABLE device (
    dev_name VARCHAR NOT NULL,
    mqtt_topic VARCHAR NOT NULL,
    dev_type INT NOT NULL,
    dev_state INT NOT NULL, -- zero or one
    PRIMARY KEY( dev_name )
);

-- example insertion
-- INSERT INTO device VALUES( 'outlet0', 'tasmota', 0 );

-- ----------------------------------------------------------------------------------------
-- Most useful example queries here
-- ----------------------------------------------------------------------------------------

-- Return list of devices for client, and respective mqtt-topic for them
-- SELECT dev_name, mqtt_topic FROM device;

-- Return mqtt_topic from device 'outlet0'
-- SELECT mqtt_topic FROM device WHERE dev_name='outlet0'

-- Return device name from mqtt_topic 'tasmota'
-- SELECT dev_name FROM device WHERE mqtt_topic='tasmota'

-- Return dev_name from device type 0 (standard toggle-able relay)
-- SELECT dev_name FROM device WHERE dev_type=0;

-- Return device count of entire database:
-- SELECT COUNT(*) FROM devices;

-- ----------------------------------------------------------------------------------------
-- Update values (mostly name) examples
-- ----------------------------------------------------------------------------------------

-- change name 'outlet0' to 'light0'
-- UPDATE device SET dev_name='light0' WHERE dev_name='outlet0';

-- update device state on device 'light0'
-- UPDATE device SET dev_toggled='ON' WHERE dev_name='light0'

-- If we had to update the mqtt_topic, this would be how.
-- in the example, mqtt_topic is changed to "newTopic" for device 'light0'
-- UPDATE device SET mqtt_topic='newTopic' WHERE dev_name='light0';

-- ----------------------------------------------------------------------------------------
-- Delete Entries example(s)
-- ----------------------------------------------------------------------------------------

-- delete light0 entry
-- DELETE FROM device WHERE dev_name='light0';