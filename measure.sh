#!/usr/bin/env bash

INTERFACE="wlp5s0"
CHANNEL="5500"

ssh fd00::101 './collector $INTERFACE'
