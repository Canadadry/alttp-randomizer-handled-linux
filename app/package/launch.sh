#!/bin/sh
cd "$(dirname "$0")" || exit 1
exec ./alttpr 2>alttpr.log
