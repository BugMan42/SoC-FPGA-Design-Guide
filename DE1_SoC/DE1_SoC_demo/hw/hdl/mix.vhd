library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.bus_input_pkg.all;

entity mix is
	generic (width_in : integer := 15;
				width_out : integer := 31);
	port (input_a : in bus_array(width_in downto 0);
			input_b : in bus_array(width_in downto 0);
			output : out bus_array(width_out downto 0);
			Start : in std_logic;
			Finish : out std_logic;
			clk   : std_logic);
end mix;

architecture structural of mix is

	signal input_a_0 : bus_array(width_in downto 0) := (others => X"00000000");
	signal input_b_0 : bus_array(width_in downto 0) := (others => X"00000000");
   signal output_0 : bus_array(width_out downto 0) := (others => X"00000000");
	signal i, cnt_a, cnt_b : integer := 0;
	signal finish_0 : std_logic := '1';
	
	type SM is(Idle, Calcul, Calcul2, Calcul3);
	signal StateM: SM;

begin
	output <= output_0;
	Finish <= finish_0;
	--input_a_0 <= input_a;
	--input_b_0 <= input_b;
	
--	process(input_a,input_b, output_0)  is
--		variable II : integer;
--		
--	begin
--		II := 0;
--		
--
----		while (II <= n) loop
----			output_0(II) <= X"01";
----			II := II + 1;
----			i <= i + 1;
----		end loop; 
--		
--		while (cnt_a < width_in) loop
--			output_0(i) <= input_a(cnt_a);
--			cnt_a := cnt_a + 1;
--			i := i + 1;
--		end loop;
--		
--		while (cnt_b < width_in) loop
--			output_0(i) <= input_b(cnt_b);
--			cnt_b := cnt_b + 1;
--			i := i + 1;
--		end loop;
------		
----		while (i < n) loop
----			output_0(cnt_a) <= X"01";--input_b_0(II);
----			cnt_a := cnt_a + 1;
----			i := i + 1;
----		end loop;
--	
--	end process;
	
	main_mix: process(clk)
		begin
			if rising_edge(clk) then
				case StateM is
					when Idle => -- Wait Commande Start
						i <= 0;
						cnt_a <= 0;
						cnt_b <= 0;
						
					if Start = '1' then
						StateM <= Calcul; 
						finish_0 <= '0';
					end if;
					
					when Calcul => 
						if (cnt_a <= width_in and cnt_b <= width_in ) then
							StateM <= Calcul;
							if (input_a(cnt_a) <=  input_b(cnt_b)) then
								output_0(i) <= input_a(cnt_a);
								cnt_a <= cnt_a + 1;
							else 
								output_0(i) <= input_b(cnt_b);
								cnt_b <= cnt_b + 1;
							end if;
							i <= i + 1;
						else 
							StateM <= Calcul2;
							finish_0 <= '1';
						end if;
					when Calcul2 => 	
						if (cnt_a <= width_in) then
							output_0(i) <= input_a(cnt_a);
							cnt_a <= cnt_a + 1;
							i <= i + 1;
							StateM <= Calcul2;
						else
							StateM <= Calcul3;
						end if;	
							
					when Calcul3 => 	
						if (cnt_b <= width_in) then
							output_0(i) <= input_b(cnt_b);
							cnt_b <= cnt_b + 1;
							i <= i + 1;
							StateM <= Calcul3;
						else
							StateM <= Idle;
							finish_0 <= '1';
						end if;	
												
					when others => null;
				
				end case;
			end if;
		end process main_mix;

	
end structural;