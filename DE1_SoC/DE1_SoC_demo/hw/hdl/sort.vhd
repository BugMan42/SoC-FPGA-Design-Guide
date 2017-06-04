library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

ENTITY sort IS
	PORT(in1, in2, in3, in4: IN unsigned (31 DOWNTO 0);
		  out1,out2,out3,out4: OUT unsigned(31 DOWNTO 0));
END sort;


ARCHITECTURE merge OF sort IS
	
	component sort_2 is
		port (a : in unsigned (31 downto 0);
				b : in unsigned(31 downto 0);
				sort_op_up : out unsigned(31 downto 0);
				sort_op_down : out unsigned(31 downto 0));
	end component;
	
	COMPONENT merge2 is
	PORT (in2_1,in2_2,in2_3,in2_4: IN unsigned(31 DOWNTO 0);
			out2_1,out2_2, out2_3, out2_4: OUT unsigned(31	DOWNTO 0));
	END COMPONENT;

	SIGNAL a,b,c,d: unsigned (31 DOWNTO 0);

	BEGIN
		
		lev1_1: sort_2 PORT MAP (in1, in2, a, b);
		lev1_2: sort_2 PORT MAP (in3, in4, c, d);
		lev2_1: merge2 PORT MAP (a, b, c, d, out1, out2, out3, out4);

END merge;	