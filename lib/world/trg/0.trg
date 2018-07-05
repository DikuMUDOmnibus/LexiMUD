#1
medallion~
c 1 0
touch~
if (%arg% == (office))
    set dest 1210
elseif (%arg% == (enterance))
    set dest 3001
else
    send %actor% Touch what?
    halt
end
echoaround %actor% %actor.name% touches the medallion.
send %actor% You touch the medallion.
echoaround %actor% %actor.name% disappears in a puff of smoke.
teleport %actor% %dest%
force %actor% look
echoaround %actor% %actor.name% appears in a blinding flash of light.
~
#20
practice guildmaster~
c 0 0
practice~
if (%actor.canbeseen%)
  if (%actor.class% == %self.class%)
    load m 20
    force %actor% practice %arg%
    purge systemgm
  else
    tell %actor% I can not train you.
  end
else
  say I do not deal with someone I can not see.
end
return 1
~
#98
WARNING drop~
h 100 1
~
if %actor.level% <= 100
return 0
end
~
#99
WARNING give~
i 100 1
~
if %actor.level% <= 100
return 0
end
~
$~
