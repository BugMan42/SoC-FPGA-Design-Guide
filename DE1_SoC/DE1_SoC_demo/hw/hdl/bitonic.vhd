library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.bus_input_pkg.all;

entity bitonic is 
	port (input : in bus_array(15 downto 0);
			output : out bus_array(15 downto 0));
end bitonic;

architecture structural of bitonic is
	component sort_2 is
	port (a : in unsigned (31 downto 0);
			b : in unsigned(31 downto 0);
			sort_op_up : out unsigned(31 downto 0);
			sort_op_down : out unsigned(31 downto 0));
	end component;

	signal stage1_op,stage2_op,stage3_op,stage4_op,stage5_op,stage6_op,stage7_op,stage8_op,stage9_op : bus_array(15 downto 0) := (others => X"00000000");

begin
	-------------------------------------------------------------------------------------------
	gen_reg0: for i in 0 to 7 generate
	 sort1 : sort_2 port map (input(2*i),input((2*i)+1),stage1_op(2*i),stage1_op((2*i)+1));
	end generate gen_reg0;
	-------------------------------------------------------------------------------------------
	gen_reg1: for i in 0 to 3 generate
	 sort2 : sort_2 port map (stage1_op(4*i),stage1_op((4*i)+3),stage2_op(4*i),stage2_op((4*i)+3));
	 sort3 : sort_2 port map (stage1_op((4*i)+1), stage1_op((4*i)+2), stage2_op((4*i)+1), stage2_op((4*i)+2));
	end generate gen_reg1;

	gen_reg2: for i in 0 to 7 generate
	 sort4 : sort_2 port map (stage2_op(2*i),stage2_op((2*i)+1),stage3_op(2*i),stage3_op((2*i)+1));
	end generate gen_reg2;
	-------------------------------------------------------------------------------------------
	gen_reg3: for i in 0 to 1 generate
	 sort5 : sort_2 port map (stage3_op(8*i), stage3_op((8*i)+7),stage4_op(8*i), stage4_op((8*i)+7));
	 sort6 : sort_2 port map (stage3_op((8*i)+1), stage3_op((8*i)+6),stage4_op((8*i)+1), stage4_op((8*i)+6));
	 sort7 : sort_2 port map (stage3_op((8*i)+2), stage3_op((8*i)+5),stage4_op((8*i)+2), stage4_op((8*i)+5)); 
	 sort8 : sort_2 port map (stage3_op((8*i)+3), stage3_op((8*i)+4),stage4_op((8*i)+3), stage4_op((8*i)+4));
	end generate gen_reg3;

	gen_reg4:for i in 0 to 3 generate
	 sort9 : sort_2 port map (stage4_op(4*i), stage4_op((4*i)+2),stage5_op(4*i), stage5_op((4*i)+2));
	 sort10 : sort_2 port map (stage4_op((4*i)+1), stage4_op((4*i)+3),stage5_op((4*i)+1), stage5_op((4*i)+3));
	end generate gen_reg4;

	gen_reg5:for i in 0 to 7 generate
	 sort11 : sort_2 port map (stage5_op(2*i), stage5_op((2*i)+1),stage6_op(2*i), stage6_op((2*i)+1));
	end generate gen_reg5;
	-------------------------------------------------------------------------------------------
	gen_reg6: for i in 0 to 7 generate
	 sort12 : sort_2 port map (stage6_op(i), stage6_op((i+15)-(2*i)),stage7_op(i), stage7_op((i+15)-(2*i)));
	end generate gen_reg6;

	gen_reg7: for i in 0 to 3 generate
	 sort13 : sort_2 port map (stage7_op(i), stage7_op(i+4),stage8_op(i), stage8_op(i+4));
	 sort14 : sort_2 port map (stage7_op(i+8), stage7_op(i+12),stage8_op(i+8), stage8_op(i+12));
	end generate gen_reg7;

	gen_reg8:for i in 0 to 7 generate
	 even_generate :if (i=0 or i=2 or i=4 or i=6) generate
	  sort15 : sort_2 port map (stage8_op(2*i), stage8_op((2*i)+2),stage9_op(2*i), stage9_op((2*i)+2));
	 end generate even_generate;
	 
	 odd_generate :if (i=1 or i=3 or i=5 or i=7) generate
	  sort16 : sort_2 port map (stage8_op((2*i)-1), stage8_op((2*i)+1),stage9_op((2*i)-1), stage9_op((2*i)+1));
	 end generate odd_generate;
	end generate gen_reg8;

	gen_reg9:for i in 0 to 7 generate
	 sort17 : sort_2 port map (stage9_op(2*i), stage9_op((2*i)+1), output(2*i), output((2*i)+1));
	end generate gen_reg9;
-------------------------------------------------------------------------------------------
end structural;