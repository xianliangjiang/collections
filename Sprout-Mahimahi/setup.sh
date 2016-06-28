THIS_SCRIPT=$(cd "$(dirname "${BASH_SOURCE}")" ; pwd -P)
export SPROUT_MODEL_IN="$THIS_SCRIPT/alfalfa/src/examples/sprout.model"
echo $SPROUT_MODEL_IN
export SPROUT_BT2="$THIS_SCRIPT/alfalfa/src/examples/sproutbt2"
echo $SPROUT_BT2

