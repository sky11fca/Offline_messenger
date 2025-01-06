DROP TABLE friendlist;
DROP TABLE chatlogs;
DROP TABLE users;

CREATE TABLE users(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL
);

CREATE TABLE friendlist(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL,
    friend TEXT NOT NULL,

    FOREIGN KEY(username) REFERENCES users(username),
    UNIQUE(username, friend)
);

CREATE TABLE chatlogs(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sender TEXT NOT NULL,
    receiver TEXT NOT NULL,
    message TEXT NOT NULL,

    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
);