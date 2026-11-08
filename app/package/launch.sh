#!/bin/sh
cd "$(dirname "$0")" || exit 1
exec ./zrando 2>zrando.log
