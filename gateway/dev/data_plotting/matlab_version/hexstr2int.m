function retInteger = hexstr2int(m_text)
%     retInteger=0;
%     text=upper(text);
%     for i=1:size(text,2)
%         n=base2dec(text(i), 16);
%         retInteger=retInteger + n*(16^(size(text,2)-i));
%     end
    m_text=upper(m_text);
    retInteger=sscanf(m_text,'%x');