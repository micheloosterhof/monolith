# Topics
# sub-areas inside forums.

# DROP TABLE topics;

CREATE TABLE topics (

# topic name/number
   forum_id	INT UNSIGNED NOT NULL,
   topic_id	INT UNSIGNED NOT NULL,

   name		VARCHAR(40) NOT NULL,

   frozen	ENUM( 'y','n' ) DEFAULT 'n',
   hidden	ENUM( 'y','n' ) DEFAULT 'n',

# timestamp
   stamp	TIMESTAMP,

   PRIMARY KEY ( forum_id, topic_id )
)