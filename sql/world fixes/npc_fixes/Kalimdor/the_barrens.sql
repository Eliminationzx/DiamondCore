
-- Ratchet Bruiser (3502)
UPDATE creature_template SET AIName='SmartAI', ScriptName='' WHERE entry=3502;
DELETE FROM smart_scripts WHERE entryorguid=3502 AND source_type=0;
INSERT INTO smart_scripts VALUES (3502, 0, 0, 0, 25, 0, 100, 0, 0, 0, 0, 0, 11, 18950, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Ratchet Bruiser - On Reset - Cast Invisibility and Stealth Detection');
INSERT INTO smart_scripts VALUES (3502, 0, 1, 0, 0, 0, 100, 0, 0, 3000, 15000, 20000, 11, 38661, 32, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Ratchet Bruiser - In Combat - Cast Net');
INSERT INTO smart_scripts VALUES (3502, 0, 2, 0, 6, 0, 100, 0, 0, 0, 0, 0, 149, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Ratchet Bruiser - On Death - Send Zone Under Attack');
