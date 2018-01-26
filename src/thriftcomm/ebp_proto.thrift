namespace cpp ebpannounce

struct applist {
  1: required string name,
  2: required string command,
  3: optional string comment,
}

exception applistfailure {
  1: string failmsg,
}

service announcelist {
  bool sendlist(1:applist apps) throws (1:applistfailure ouch);
}
