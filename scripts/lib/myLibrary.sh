################################################################################################################
# util functions
################################################################################################################
# use: array_contains elem array
function array_contains() {
    set +e
    local e match=$1
    shift # removes first argument
    # equivalent to for e in $@
    for e; do
        [[ "$e" == "$match" ]] && echo "found" && return 0
    done
    echo "nope"
    return 1
}
################################################################################################################
# echo functions
################################################################################################################
function repeat_char() {
    local char=$1
    local count=$2
    local i=0
    while [ $i -lt $count ]; do
        echo -n "$char"
        ((i = i + 1))
    done
}

function title_in_center() {
    local text=$1
    local terminalCols=$(tput cols -T xterm || echo 80)  # Default to 80 columns if tput fails
    echo "$(repeat_char '#' $terminalCols)"
    echo -e "$(repeat_char ' ' $(((terminalCols - ${#text}) / 2 - 1))) $text"
    echo "$(repeat_char '#' $terminalCols)"
}

function echo_in_center() {
    local terminalCols=$(tput cols -T xterm || echo 80)
    # les if sont juste là pour les options -n et -e de la commande echo
    if [ $# -eq 2 ]; then
        local text=$2
        if [ $1 == "-ne" ]; then
            echo -ne "$(repeat_char ' ' $(((terminalCols - ${#text}) / 2 - 1))) $text"
        elif [ $1 == "-e" ]; then
            echo -e "$(repeat_char ' ' $(((terminalCols - ${#text}) / 2 - 1))) $text"
        else
            echo "$(repeat_char ' ' $(((terminalCols - ${#text}) / 2 - 1))) $text"
        fi
    elif [ $# -eq 1 ]; then
        local text=$1
        echo "$(repeat_char ' ' $(((terminalCols - ${#text}) / 2 - 1))) $text"
    else
        echo "error in arguments"
        exit 1
    fi
}

function echo_progressBar() {

    local progress=$1
    local fullProgress=$2
    local terminalCols=$(($(tput cols -T xterm || echo 80) - 10)) # pour eviter la surcharge de la ligne et ainsi le retour carriage n'aura plus aucun effet

    percentage=$(((progress * 100) / (fullProgress)))
    # terminalCols => 100%
    # progressBar => $percentage
    ratio=$(((percentage * terminalCols) / 100))
    progressBar=$(repeat_char = $ratio)
    left=$((terminalCols - ratio))
    leftBar=$(repeat_char ' ' $left)
    echo_in_center -ne "[$progressBar>$leftBar]($percentage%)\r"
}
