#!/bin/bash

if [ ! -f gmic_survey.raw ]; then
    touch gmic_survey.raw
    chmod a+rw gmic_survey.raw
fi

echo $QUERY_STRING >> gmic_survey.raw

echo "Content-type: text"
echo
echo "#@gmic"
echo "gmic_survey_check:"
