#1200
test~
d 1 0
hi bye seeya hmph~
switch %speech.1%
  case hi
    say hi!
    set x 1
    while (%x% < 1000)
      say x = %x%
      if (%x% == 200)
        say Uh-oh, x = %x%!
        break
      end
    eval x %x% + 1
    done
    break
  case bye
  case seeya
    say later!
    break
  default
    say Whatever.
done
~
#1201
OOC script~
b 4 0
none~
echo A large marble fountain flow gently befor you, splashing small droplets of water about the Atrium.
asound You hear the gentle sounds of running water from nearby. You suddenly feel at peace with all things.
wait 2
echo You suddenly feel more at peace with the outside world.
~
#1202
Cherry Bloosom~
b 20 0
none~
echo The small `wwhite`n petals of a `rcherry`n blossom blow about, as the wind gently lifts them into the air, only for them to gracefuly float back to the earth.
wait 20
purge self
~
#1203
tree load~
b 1 0
none~
load obj 1206
echo The wind about you picks up sudden speed stiring the `rCherry `wTrees `nin the atrium
wait 1
echo A white cherry blossom floats gently to earth, settling on the ground nearby.
~
#1204
Immortal Sack Get Script~
g 0 1
~
if %actor.staff%
return 1
else
send %actor% Thats for immortals only! Hands off!
echoaround %actor% %actor.name% tried to take the Immortal's sack, Slay him!
return 0
end
~
#1205
test grenade launcher script~
c 7 1
launch~
if ((north south east west up down) /= (%arg.1%))
     halt
end
send %actor% Test
send %actor% You said: %arg.1
~
#1206
squeaky chair~
d 100 1
none~
switch (%random.5%)
  case 1
    echo %self.shortdesc% squeaks loudly.
    break
  case 2
  case 3
    echo %self.shortdesc% squeaks quietly.
    break
done
~
#1209
test script~
c 0 0
enter~
say %arg.1%
say %arg.2% 
~
#1210
crashtest script~
g 100 2
none~
purge %actor%
~
#1211
Jake's Kill-script~
k 100 0
~
hiss %actor%
if (%actor.npc%)
purge %actor%
else
damage %actor% 10000
endif
~
#1212
CMEQ Damage Script~
b 50 1
~
echo %object.name% is making some funny sounds!
wait 2
echo %object.name% sends streaks of enegery in all directions, ouch!
damage all 1500
halt
end
~
#1218
Silence Office Guard~
ag 100 2
~
if (%actor.level% >= 90)
  wait 1
  send %actor% Welcome to the lair of Silence.
else
  send %actor% You must be level 90 to enter that room.
  return 0
halt
end
~
#1230
Charlie script~
g 100 0
~
if (%actor.name% == Frodo)
stand
follow %actor%
echoaround %actor% %self.name% smiles up at %actor.name%
send %actor% Your dog starts to follow you.
heal self 10000
end
~
#1231
Charlie Voice Command <main>~
d 1 0
sit stand go.home destroy mutilate roll.over play.dead fetch bury wish.of.luck protect.me~
switch %speech.1%
case sit
if (%actor.name% == frodo)
nod
sit
follow self
else
say Hah! Thats funny.
halt
end
break
case stand
if (%actor.name% == frodo)
stand
smile
follow %actor%
else
laugh
halt
end
break
case destroy
if (%actor.name% == frodo)
damage all 10000
heal %self.name% 20000
cackle
else
say How pathetic.
thwap %actor%
halt
end
break
case go.home
if (%actor.name% == frodo)
say Right Away Master.
echo Charlie dissapears in a swirling mist.
teleport %self.name% 1242
else
say Your not my master!!
wait 4
teleport %actor% 0
send %actor% Charlie slashes you across the chest!
damage %actor% 10000
halt
end
break
case mutilate
heal %self.name% 1000000
grin
halt
end
break
case roll.over
if (%actor.name% == frodo)
emote rolls over onto his back and %actor.name% scratches his stomach.
wait 1
emote shakes his leg and get back on his feet.
else
if (%actor.staff%)
echo Charlie does a flip and finally rolls over on his back and smiles.
else
echo Charlie rolls over and does a trick.
heal %actor% 10
halt
end
break
case play.dead
if (%actor.name% == frodo)
echo Charlie gets a knife out and does a great acting job as he thrusts it thru his heart!
wait 1p
chuckle
else
say You want me to die?!
emote frowns and turns his head away.
halt
end
break
case fetch
if (%actor.name% == frodo)
echo %actor.name% throws a stick in the distance and Charlie runs after it and comes back with the stick in his mouth.
emote looks faithfully up at his master.
else
if (%actor.staff%)
echo Charlie runs after %actor.name%s stick and returns with it in his mouth.
say Im happy to be of service!
else
echo Charlie goes for the stick that %actor.name% threw and returns with it in his mouth.
grin
halt
end
break
case bury
if (%actor.name% == frodo)
nod
emote creates a giant bone from thin air!
emote licks the bone a couple of times and burys it in a giant hole.
smile %actor%
else
if (%actor.staff%)
frown
say I'm sorry but I don't have a bone to bury
poke %actor
else
chuckle
say What bone?
emote looks around
say I don't see a bone to bury around here.
halt
end
break
case protect.me
if (%actor.name% == frodo)
say Right away master!
teleport %actor% 1226
sit
say You fools!
echo Charlie calls forth the power of his master!
wait 1
echo Giant red comets appear and start to destroy the area!
wait 2
damage all 6000
teleport self 1226
say I have protected my master.
smile %actor%
heal %actor% 5000
sit
else
say Hah! Protect you?!
halt
end
break
case wish.of.luck
if (%actor.name% == frodo)
say Yes sir.
echo Charlie calls forth the power of the Immortals and heals you!
wait 1
heal all 5000
echo You feel blessed!
else say Only my master can order me to do that!
halt
end
~
#1232
Charlie Random Heal~
b 100 0
~
heal %self.name% 100000
return
halt
end
~
#1233
charlie slap~
e 0 0
You are slapped~
if (%actor.name% == Frodo)
frown
else
slap %actor%
eye
halt
end
~
#1234
charlie pet~
e 0 0
pets you lovingly~
echoaround %actor% %self.name% licks %actor.name%'s hand.
send %actor% %self.name% licks your hand.
halt
end
~
#1235
teleport defence~
f 100 0
~
if (%actor.name% == Frodo)
say why master... why?
else
load mob 1230
send %actor% Your gonna burn in hell!!
teleport %actor% 1899
end
~
#1236
Charlie Hastur Counter~
c 0 0
nukenuke~
if (%actor.name% == Frodo)
purge
else
teleport %actor% 0
halt
end
~
#1237
Attack teleport~
k 30 0
~
if (%actor.name% == Frodo)
grin
else
if (%actor.name% == Warlord)
chuckle
else
if (%actor.staff%)
send %actor% I dont think Frodo would like that.
teleport %actor% 1899
teleport %self% 1226
heal self 100000
else
cackle
say You weak pathetic fool!
damage %actor% 2000
wait 1
damage %actor% 3000
echoaround %actor% %actor.name% dissapears into a swirl of mist!
teleport %actor% 1899
send %actor% Charlie says, "Next time think before you fight me"
heal self 100000
say Let that be a lesson to all of you!
damage %actor% 5000
halt
end
~
#1239
Chalrie control script~
c 0 0
ccontrol~
if (%actor.name% == frodo)
send %actor% Yes sir.
force %self% %arg%
halt
end
~
#1240
death to charlie script~
c 0 0
death.to~
if (%actor.name% == Frodo)
set hellomyroom %self.room%
say I shall destroy %arg% as directed by my master!
snarl
wait 1
send %arg% You are slashed by Charlie!
damage %arg% 1000
wait 1
send %arg% Charlie pounds on you as he stares into your soul!
damage %arg% 1000
teleport %arg% 0
damage %arg% 10000
teleport self %hellomyroom%
say I have disposed of %arg% as directed.
sit
halt
end
~
#1241
Suppress purge charlie~
c 0 0
purge~
if (%actor.name% == frodo)
smile
return 0
else
grin
end
~
#1242
Pick up Slay~
g 100 1
~
if (%actor.staff% || %actor.level% ==100)
send %actor% You feel the power surge thru you.
else
send %actor% You cant handle the power of the slay command!!
damage %actor% 10000
return 0
end
~
#1243
Charlie Kill Supress~
c 0 0
~
if (%actor.name% == frodo)
smile
return 0
else
say No killing while i am here.
end
~
#1252
Charlie Desperate~
l 50 0
~
say `fYou think you got me eeh?!`n
heal %self.name% 100000
echo `rCharlie`n invokes the power of `bFrodo`n and heals himself!
halt
end
~
#1253
Charlie T-Bone~
j 100 0
~
if (%object.vnum% == 1899)
wait 1
drool
say MmMmM! a steak!
eat %object
say Thanks! May The Gods Shine on you!
send %actor% You feel the blessing of the Gods! You are healed!
heal %actor% 5000
else
if (%actor.name% == Frodo)
else
say Why would I want that?
purge %object
halt
end
~
#1255
Trash Bin Purge~
b 100 1
~
junk %contents
halt
end
~
#1260
selfdelete~
abj 25 1
~
echo %self.name% disappears!
purge %self%
return 1
~
#1265
test script~
c 0 0
test~
info This is just a test
~
#1296
Digger's shield script~
c 0 1
0~
gecho The legend Digger adorns a great shield on his arm, and lets out 
a vicious battlecry heard across the realm.
~
#1298
mission flag oddjob script~
c 2 1
put flag~
return 0
zoneecho (%self.room% / 100) %actor.name% is trying to cheat!
~
#1299
poke script~
e 0 0
pokes you in the~
snicker
poke %actor%
~
$~
