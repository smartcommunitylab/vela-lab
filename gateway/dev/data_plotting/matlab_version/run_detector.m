#!/usr/bin/flatpak run org.octave.Octave -qf

arg_list = argv ();
run_from_cmd_line=~(size(arg_list,1)<1 ||  strcmp(arg_list{1},"--gui"));
if ~run_from_cmd_line
  #filename="vela-09082019/20190809-141430-contact.log"; #this is no more supported because the timestamp format is different
  filename="../../sink_integration/log/20191122-140928-contact.log";
  warning("Filename not provided, analyzing hardcoded filename: %s.",filename);
else
  filename=arg_list{1};
endif


if run_from_cmd_line
  #default configuration when running from comand line
  text_verbosity=1;
  image_verbosity=0; #plots work nicely only with only one beacon (use id_to_analyze to filter the desired one)

  id_to_analyze={
  };
else
  text_verbosity=2;
  image_verbosity=1; #plots work nicely only with only one beacon (use id_to_analyze to filter the desired one)
  
  id_to_analyze={
    '0000FF0000F5';
    %'0000FF0000F2';
    %'000000000065';
    %'0000FF0000F3';
  };
endif
  
id_to_remove={
  %'0000FF0000F3';
  %'0000FF0000F4';
  %'0000FF0000F5';
  %'0000FF0000F6';
  %'00000000004F';
  %'000000000050';
  %'000000000051';
  %'000000000052';
  %'000000000053';
  %'000000000054';
  %'000000000055';
  %'000000000056';
  %'000000000057';
  %'000000000058';
  %'000000000059';
  %'00000000005A';
  %'00000000005B';
  %'00000000005C';
  %'00000000005D';
};

evts=proximity_detect(filename,text_verbosity,image_verbosity,id_to_analyze,id_to_remove);
if text_verbosity>=2
  printf("%c",evts);
end
