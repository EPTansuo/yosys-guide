/*
ISC License

Copyright (C) 2024 Microchip Technology Inc. and its subsidiaries

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

module ram_TDP (clka,clkb,wea,addra,dataina,qa,web,addrb,datainb,qb);
parameter addr_width = 10;
parameter data_width = 2;
input clka,clkb,wea,web;
input [data_width - 1 : 0] dataina,datainb;
input [addr_width - 1 : 0] addra,addrb;
output reg [data_width - 1 : 0] qa,qb;
reg [addr_width - 1 : 0] addra_reg, addrb_reg;
reg [data_width - 1 : 0] mem [(2**addr_width) - 1 : 0];

always @ (posedge clka)
    begin
        addra_reg <= addra;
        if(wea)
            mem[addra] <= dataina;
    end

always @ (posedge clkb)
begin
addrb_reg <= addrb;
if(web)
mem[addrb] <= datainb;
end

always @ (posedge clka)
begin
if(~wea)
qa <= mem[addra];
else qa <= dataina;
end

always @ (posedge clkb)
begin
if(~web)
qb <= mem[addrb];
else qb <= datainb;
end
endmodule
