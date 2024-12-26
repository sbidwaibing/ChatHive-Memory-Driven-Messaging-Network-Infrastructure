-- This file was auto-generated from schema.sql.cpp

-- minimally normalized schema



  CREATE TABLE IF NOT EXISTS chats ( 
    id INTEGER PRIMARY KEY, 
    user TEXT, 
    room TEXT, 
    message TEXT, 
    creationTime INTEGER 
      DEFAULT (CAST(1000*(STRFTIME('%s', 'NOW') + 
		      MOD(STRFTIME('%f', 'NOW'), 1)) 
		    AS INTEGER))		  
  ); 
  CREATE INDEX IF NOT EXISTS roomx ON chats(room); 



  CREATE TABLE IF NOT EXISTS topics ( 
    chatId INTEGER, 
    topic TEXT, 
    FOREIGN KEY(chatId) REFERENCES chats(id) 
  ); 
  CREATE INDEX IF NOT EXISTS topicx ON topics(topic); 
  CREATE UNIQUE INDEX IF NOT EXISTS utopicx ON topics(chatId, topic);

