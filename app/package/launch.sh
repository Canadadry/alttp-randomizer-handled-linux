#!/bin/sh
cd "$(dirname "$0")" || exit 1
exec ./zrando --rotate 180 2>zrando.log
