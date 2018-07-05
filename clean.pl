#!/usr/bin/perl
#  House Cleaner v.1, by Pat Dughi 5-30-97
# kinda a substitute for purgeplay once you get that fancy ascii pfile
# thing and they aint removed nay more.

$LEVEL_DEL=10;               # level to delete after 30 days.

#general declares
$DELETE_BIT=1024;          #this is 2^10 or (1 << 10)
$NODELETE_BIT=8192;         #this is 2^13 or (1 << 13)

$START_PLAYER_DIR="a";

$PLAYER_DIR="lib/pfiles";
$TEMP_DIR="/tmp";

$PLAYER_INDEX="lib/pfiles/plr_index";

# find_and_remove subroutine
# Find out if a named player needs to be deleted!

sub find_and_remove {
  my $character_name = shift(@_);
  $delete=0;
  $nodelete=0;

  if ($character_name eq "." || $character_name eq ".." || '.objs' eq substr($character_name, -5)) {
    return 0;
  }

  $lastnum = 0;
  $lvlnum = 0;

  open(PFILE, "$PLAYER_DIR/$START_PLAYER_DIR/$character_name");

  while(<PFILE>) {
    if (/Act /) {
      ($act,$colon,$actnum)=split(/\s/,$_,3);
      if ($actnum & $DELETE_BIT && !($actnum & $NODELETE_BIT)) {
        $delete=1;
      }
      if ($actnum & $NODELETE_BIT) {
        $nodelete = 1;
      }
    }
    if (/Id  /) {
      ($id,$colon,$idnum)=split(/\s+/,$_,3);
      chop($idnum);
    }
    if (/Last/) {
      ($last,$lastnum)=split(/\s+/,$_,2);
      chop($lastnum);
    }
    if (/Levl/) {
      ($levl,$lvlnum)=split(/\s+/,$_,2);
      chop($lvlnum);
      if ($lvlnum < 1) {
        $delete = 3;
      }
    }
  }
   if (((time() - $lastnum) > (60*60*24*15)) && ($lvlnum < 5)) {
     #then, its more than 1 months
     $delete=2;
   }
   if (((time() - $lastnum) > (60*60*24*30)) && ($lvlnum < 15)) {
     #then, its more than 1 months
     $delete=2;
   }
   if (((time() - $lastnum) > (60*60*24*30*2)) && ($lvlnum < 25)) {
     #then, its more than 3 months
     $delete=2;
   }
   if ((time() - $lastnum) > (60*60*24*30*3)) {
     #then, its more than 6 months
     $delete=2;
   }
   if (time() < $lastnum) {
     print($character_name "is corrupt!");
     $delete=4;
   }
  close(PFILE);

  if($delete && !$nodelete) {
    if ($delete==1) {
      print("Deleting $character_name, delete bits set.\n");
    }
    if ($delete==2) {
      print("Deleting $character_name, old file.\n");
    }
    if ($delete==3) {
      print("Deleting $character_name, bad file (Level < 1).\n");
    }
    if ($delete==4) {
      print("Deleting $character_name, bad file (newer than now)\n");
    }
    unlink("$PLAYER_DIR/$START_PLAYER_DIR/$character_name");

    if(-e ("$PLAYER_DIR/$START_PLAYER_DIR/$character_name" . ".objs")) {
      unlink("$PLAYER_DIR/$START_PLAYER_DIR/$character_name" . ".objs");
    }

    return 0; # found and removed.
  }
  return $idnum; #  found, is okay for insertion in index.
}

#this is the main function.

print("House cleaning started.\n");
open(NEW_PLAYER_INDEX, "> /tmp/tmpplr_index") or die "Failure to open tmp file\n";

while($START_PLAYER_DIR ne "aa") {
  opendir TEMPDIR, "$PLAYER_DIR/$START_PLAYER_DIR";
  @dir_list= readdir TEMPDIR;
  closedir TEMPDIR;

  foreach $character (@dir_list) {

    if(find_and_remove($character)) {
      $their_id=find_and_remove($character);

      print NEW_PLAYER_INDEX "$their_id\t$character\n";
    }
  }
  $START_PLAYER_DIR++;

}

close(NEW_PLAYER_INDEX);

#just for reading.

open(NEW_PLAYER_INDEX,  "/tmp/tmpplr_index") or die "Lost tempfile!\n";
@presort=<NEW_PLAYER_INDEX>;
close(NEW_PLAYER_INDEX);

sub numfirst {

  ($numbera,$crapa) =split(/\t/,$a);
  ($numberb,$crapb) =split(/\t/,$b);
 $numbera <=> $numberb;
}
@sorted=sort numfirst @presort;

open(PLAYER_INDEX_T,  "> $PLAYER_INDEX");
foreach $line (@sorted) {
  print PLAYER_INDEX_T $line;
}
close(PLAYER_INDEX_T);

print("All done!\n");

