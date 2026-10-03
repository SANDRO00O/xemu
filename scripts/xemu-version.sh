#!/bin/bash

dir="$1"

XEMU_VERSION="0.0.0"
XEMU_COMMIT=""
XEMU_DATE=""

if test -d "$dir"; then
  XEMU_DATE=$(date -u 2>/dev/null || printf 'unknown')
  XEMU_COMMIT=$(git -C "$dir" rev-parse HEAD 2>/dev/null || printf '')
  version=$(git -C "$dir" describe --tags --match 'v*' 2>/dev/null || printf '')
  if test -n "$version"; then
    XEMU_VERSION=${version#v}
  fi
fi

get_version_field() {
  printf '%s\n' "${XEMU_VERSION}-0" | cut -d- -f"$1"
}

get_version_dot() {
  get_version_field 1 | cut -d. -f"$1"
}

XEMU_VERSION_MAJOR=$(get_version_dot 1)
XEMU_VERSION_MINOR=$(get_version_dot 2)
XEMU_VERSION_PATCH=$(get_version_dot 3)
XEMU_VERSION_COMMIT=$(get_version_field 2)

test -n "$XEMU_VERSION_MAJOR" || XEMU_VERSION_MAJOR=0
test -n "$XEMU_VERSION_MINOR" || XEMU_VERSION_MINOR=0
test -n "$XEMU_VERSION_PATCH" || XEMU_VERSION_PATCH=0
test -n "$XEMU_VERSION_COMMIT" || XEMU_VERSION_COMMIT=0

cat <<EOF
#define XEMU_VERSION       "$XEMU_VERSION"
#define XEMU_VERSION_MAJOR $XEMU_VERSION_MAJOR
#define XEMU_VERSION_MINOR $XEMU_VERSION_MINOR
#define XEMU_VERSION_PATCH $XEMU_VERSION_PATCH
#define XEMU_VERSION_COMMIT $XEMU_VERSION_COMMIT
#define XEMU_COMMIT        "$XEMU_COMMIT"
#define XEMU_DATE          "$XEMU_DATE"
EOF

exit 0
