#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="${1:-/home/js/coding/assets}"
DEST_DIR="${2:-/home/js/coding/elysium/assets}"

if ! command -v unzip >/dev/null 2>&1; then
  echo "unzip is required but not installed."
  exit 1
fi

if [[ ! -d "${SOURCE_DIR}" ]]; then
  echo "Source directory not found: ${SOURCE_DIR}"
  exit 1
fi

mkdir -p "${DEST_DIR}"/{characters,environment,props,tiles,animations,misc}

categorize_zip() {
  local name_lower="$1"
  if [[ "${name_lower}" == *adventurer* || "${name_lower}" == *skeleton* ]]; then
    echo "characters"
  elif [[ "${name_lower}" == *animat* ]]; then
    echo "animations"
  elif [[ "${name_lower}" == *dungeon* || "${name_lower}" == *tile* ]]; then
    echo "tiles"
  elif [[ "${name_lower}" == *forest* || "${name_lower}" == *nature* ]]; then
    echo "environment"
  elif [[ "${name_lower}" == *weapon* || "${name_lower}" == *tool* || "${name_lower}" == *resource* || "${name_lower}" == *bits* ]]; then
    echo "props"
  else
    echo "misc"
  fi
}

shopt -s nullglob
for zip_path in "${SOURCE_DIR}"/*.zip; do
  zip_name="$(basename "${zip_path}")"
  name_no_ext="${zip_name%.zip}"
  name_lower="$(echo "${name_no_ext}" | tr '[:upper:]' '[:lower:]')"

  category="$(categorize_zip "${name_lower}")"
  pack_folder="$(echo "${name_no_ext}" | tr ' ' '_')"
  target_dir="${DEST_DIR}/${category}/${pack_folder}"

  mkdir -p "${target_dir}"
  echo "Extracting ${zip_name} -> ${target_dir}"
  unzip -oq "${zip_path}" -d "${target_dir}"
done

echo "Asset organization complete at: ${DEST_DIR}"
