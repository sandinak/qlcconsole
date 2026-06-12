project(effectscripts)

set(SCRIPT_FILES
    breathe.js
    color-chase.js
    fan.js
    fire.js
    followspot.js
    lightning.js
    orbit.js
    rainbow.js
    ramp-chase.js
    sparkle.js
    spatial-gradient.js
    strobe.js
    wander.js
)

install(FILES ${SCRIPT_FILES} DESTINATION ${INSTALLROOT}/${EFFECTSCRIPTDIR})
