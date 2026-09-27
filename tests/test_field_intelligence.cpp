#include "field_intelligence.h"
int main(){field::WatchStats w;w.reset(433920,0);w.sample(-60,1000,-75);w.sample(-90,2000,-75);return(w.detections==1&&w.strongest==-60&&field::supported(433920)&&!field::supported(1090000)&&field::rssiBar(-70)==50)?0:1;}
