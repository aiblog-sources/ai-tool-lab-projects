#!/bin/bash
# Every 30 s: RSS (kB) and cumulative CPU ticks of the Hermes gateway (with children) and busybox crond.
B=$1; out=$B/results/resources.tsv; hz=$(getconf CLK_TCK)
echo -e "epoch\tgateway_rss_kb\tgateway_cpu_s\tcrond_rss_kb\tcrond_cpu_s" > $out
for i in $(seq 1 44); do
  g=$(cat $B/gateway.pid); c=$(pgrep -f '^crond -f -l 8' | head -1)
  gr=$(awk '/VmRSS/{print $2}' /proc/$g/status 2>/dev/null); gc=$(awk -v hz=$hz '{print ($14+$15)/hz}' /proc/$g/stat 2>/dev/null)
  cr=$(awk '/VmRSS/{print $2}' /proc/$c/status 2>/dev/null); cc=$(awk -v hz=$hz '{print ($14+$15)/hz}' /proc/$c/stat 2>/dev/null)
  echo -e "$(date +%s)\t$gr\t$gc\t$cr\t$cc" >> $out; sleep 30
done
